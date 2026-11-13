#include "ImporterPlugin.h"

#include "PluginEngine.h"

#include <chrono>
#include <cstring>
#include <wasmtime.h>

namespace EasePass::Plugin {

namespace {

using Clock = std::chrono::steady_clock;
using Val = wasmtime_component_val_t;

constexpr char TypesInterface[] = "easepass:plugin/types@1.0.0";
constexpr char LogInterface[] = "easepass:plugin/log@1.0.0";
constexpr char FilePickerInterface[] = "easepass:plugin/file-picker@1.0.0";
constexpr char ImporterInterface[] = "easepass:plugin/importer@1.0.0";

QString takeErrorText(wasmtime_error_t *error) {
    wasm_name_t message;
    wasmtime_error_message(error, &message);
    const QString text = QString::fromUtf8(message.data, qsizetype(message.size));
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return text;
}

// --- reading component values ------------------------------------------------

QString toString(const wasm_name_t &name) { return QString::fromUtf8(name.data, qsizetype(name.size)); }

const Val *field(const Val &record, const char *name) {
    if (record.kind != WASMTIME_COMPONENT_RECORD)
        return nullptr;
    const size_t length = std::strlen(name);
    for (size_t i = 0; i < record.of.record.size; ++i) {
        const wasmtime_component_valrecord_entry_t &entry = record.of.record.data[i];
        if (entry.name.size == length && std::memcmp(entry.name.data, name, length) == 0)
            return &entry.val;
    }
    return nullptr;
}

bool readString(const Val *value, QString &out) {
    if (!value || value->kind != WASMTIME_COMPONENT_STRING)
        return false;
    out = toString(value->of.string);
    return true;
}

bool readStringField(const Val &record, const char *name, QString &out) { return readString(field(record, name), out); }

bool readStringList(const Val *value, QStringList &out) {
    if (!value || value->kind != WASMTIME_COMPONENT_LIST)
        return false;
    for (size_t i = 0; i < value->of.list.size; ++i) {
        QString item;
        if (!readString(&value->of.list.data[i], item))
            return false;
        out << item;
    }
    return true;
}

// --- writing component values (ownership passes to Wasmtime) -------------------

void setString(Val &value, const QByteArray &utf8) {
    value.kind = WASMTIME_COMPONENT_STRING;
    wasm_byte_vec_new(&value.of.string, size_t(utf8.size()), utf8.constData());
}

void setEnum(Val &value, const char *caseName) {
    value.kind = WASMTIME_COMPONENT_ENUM;
    wasm_byte_vec_new(&value.of.enumeration, std::strlen(caseName), caseName);
}

void setRecordField(wasmtime_component_valrecord_entry_t &entry, const char *name) {
    wasm_byte_vec_new(&entry.name, std::strlen(name), name);
}

/// result<T, error> = err(error { code, message })
void setErrorResult(Val &value, const char *code, const QString &message) {
    Val error;
    error.kind = WASMTIME_COMPONENT_RECORD;
    wasmtime_component_valrecord_new_uninit(&error.of.record, 2);
    setRecordField(error.of.record.data[0], "code");
    setEnum(error.of.record.data[0].val, code);
    setRecordField(error.of.record.data[1], "message");
    setString(error.of.record.data[1].val, message.toUtf8());

    value.kind = WASMTIME_COMPONENT_RESULT;
    value.of.result.is_ok = false;
    value.of.result.val = wasmtime_component_val_new(&error); // moves `error` into a box
}

} // namespace

struct ImporterPlugin::Private {
    PluginInspection inspection;
    PluginHost host;
    PluginLimits limits;

    wasmtime_component_t *component = nullptr;
    wasmtime_store_t *store = nullptr;
    wasmtime_context_t *context = nullptr;
    wasmtime_component_linker_t *linker = nullptr;
    wasmtime_component_instance_t instance{};
    wasmtime_component_func_t sourcesFunc{};
    wasmtime_component_func_t importFunc{};

    Clock::time_point deadline;
    bool timedOut = false;
    bool unusable = false;

    ~Private() {
        if (linker)
            wasmtime_component_linker_delete(linker);
        if (store)
            wasmtime_store_delete(store);
        if (component)
            wasmtime_component_delete(component);
    }

    // Runs whenever the engine epoch advances while this store executes plugin code.
    static wasmtime_error_t *onEpoch(wasmtime_context_t *, void *data, uint64_t *delta,
                                     wasmtime_update_deadline_kind_t *kind) {
        auto *self = static_cast<Private *>(data);
        if (Clock::now() >= self->deadline) {
            self->timedOut = true;
            return wasmtime_error_new("plugin call timed out");
        }
        *delta = 1;
        *kind = WASMTIME_UPDATE_DEADLINE_CONTINUE;
        return nullptr;
    }

    // log: func(level: level, message: string)
    static wasmtime_error_t *hostLog(void *data, wasmtime_context_t *, const wasmtime_component_func_type_t *,
                                     Val *args, size_t argCount, Val *, size_t) {
        auto *self = static_cast<Private *>(data);
        if (argCount != 2 || args[0].kind != WASMTIME_COMPONENT_ENUM || args[1].kind != WASMTIME_COMPONENT_STRING)
            return wasmtime_error_new("log: unexpected arguments");
        if (self->host.log)
            self->host.log(toString(args[0].of.enumeration), toString(args[1].of.string));
        return nullptr;
    }

    // pick-file: func(options: pick-options) -> result<option<picked-file>, error>
    static wasmtime_error_t *hostPickFile(void *data, wasmtime_context_t *, const wasmtime_component_func_type_t *,
                                          Val *args, size_t argCount, Val *results, size_t resultCount) {
        auto *self = static_cast<Private *>(data);
        if (argCount != 1 || resultCount != 1)
            return wasmtime_error_new("pick-file: unexpected arguments");

        QString title;
        const Val *titleField = field(args[0], "title");
        if (titleField && titleField->kind == WASMTIME_COMPONENT_OPTION && titleField->of.option)
            readString(titleField->of.option, title);
        QStringList extensions;
        if (!readStringList(field(args[0], "extensions"), extensions))
            return wasmtime_error_new("pick-file: unexpected options");

        if (!self->host.pickFile) {
            setErrorResult(results[0], "unsupported", QStringLiteral("no file picker available"));
            return nullptr;
        }
        // The user may take as long as they like: dialog time does not count as plugin time.
        const Clock::time_point before = Clock::now();
        const std::optional<PluginHost::PickedFile> picked = self->host.pickFile(title, extensions);
        self->deadline += Clock::now() - before;

        Val option;
        option.kind = WASMTIME_COMPONENT_OPTION;
        option.of.option = nullptr;
        if (picked) {
            Val file;
            file.kind = WASMTIME_COMPONENT_RECORD;
            wasmtime_component_valrecord_new_uninit(&file.of.record, 2);
            setRecordField(file.of.record.data[0], "name");
            setString(file.of.record.data[0].val, picked->name.toUtf8());
            setRecordField(file.of.record.data[1], "data");
            Val &bytes = file.of.record.data[1].val;
            bytes.kind = WASMTIME_COMPONENT_LIST;
            // list<u8> is one tagged value per byte in the C API (see docs/PLUGIN_DECISION.md).
            wasmtime_component_vallist_new_uninit(&bytes.of.list, size_t(picked->data.size()));
            for (qsizetype i = 0; i < picked->data.size(); ++i) {
                bytes.of.list.data[i].kind = WASMTIME_COMPONENT_U8;
                bytes.of.list.data[i].of.u8 = uint8_t(picked->data.at(i));
            }
            option.of.option = wasmtime_component_val_new(&file);
        }
        results[0].kind = WASMTIME_COMPONENT_RESULT;
        results[0].of.result.is_ok = true;
        results[0].of.result.val = wasmtime_component_val_new(&option);
        return nullptr;
    }

    /// Defines one interface in the linker. `functions` are (name, callback) pairs.
    QString defineInterface(const char *name,
                            std::initializer_list<std::pair<const char *, wasmtime_component_func_callback_t>> functions) {
        wasmtime_component_linker_instance_t *root = wasmtime_component_linker_root(linker);
        wasmtime_component_linker_instance_t *interface = nullptr;
        wasmtime_error_t *error =
            wasmtime_component_linker_instance_add_instance(root, name, std::strlen(name), &interface);
        for (const auto &[functionName, callback] : functions) {
            if (error)
                break;
            error = wasmtime_component_linker_instance_add_func(interface, functionName, std::strlen(functionName),
                                                                callback, this, nullptr);
        }
        if (interface)
            wasmtime_component_linker_instance_delete(interface);
        wasmtime_component_linker_instance_delete(root);
        return error ? takeErrorText(error) : QString();
    }

    bool lookup(const char *function, wasmtime_component_func_t *out) {
        wasmtime_component_export_index_t *interface =
            wasmtime_component_get_export_index(component, nullptr, ImporterInterface, std::strlen(ImporterInterface));
        if (!interface)
            return false;
        wasmtime_component_export_index_t *index =
            wasmtime_component_get_export_index(component, interface, function, std::strlen(function));
        wasmtime_component_export_index_delete(interface);
        if (!index)
            return false;
        const bool found = wasmtime_component_instance_get_func(&instance, context, index, out);
        wasmtime_component_export_index_delete(index);
        return found;
    }

    /// Runs `body` (which calls into the plugin) with the timeout armed and classifies failures.
    template <typename Body> bool guarded(PluginError *error, Body body) {
        if (unusable) {
            *error = {PluginError::Kind::Unusable, {},
                      QStringLiteral("the plugin instance was stopped after an earlier failure")};
            return false;
        }
        PluginEngine::CallGuard ticking;
        timedOut = false;
        deadline = Clock::now() + std::chrono::milliseconds(limits.callTimeoutMs);
        wasmtime_context_set_epoch_deadline(context, 1);
        if (wasmtime_error_t *failure = body()) {
            // After a trap or interruption the instance state is undefined: never reuse it.
            unusable = true;
            const QString text = takeErrorText(failure);
            *error = {timedOut ? PluginError::Kind::Timeout : PluginError::Kind::Trap, {}, text};
            return false;
        }
        return true;
    }

    bool call(const wasmtime_component_func_t &function, const Val *args, size_t argCount, Val *result,
              PluginError *error) {
        return guarded(error, [&]() -> wasmtime_error_t * {
            if (wasmtime_error_t *failure =
                    wasmtime_component_func_call(&function, context, args, argCount, result, 1))
                return failure;
            return wasmtime_component_func_post_return(&function, context);
        });
    }
};

namespace {

/// Unwraps result<T, error>; returns the ok payload or sets `error`.
const Val *unwrapResult(const Val &value, PluginError *error) {
    if (value.kind != WASMTIME_COMPONENT_RESULT) {
        *error = {PluginError::Kind::Host, {}, QStringLiteral("expected a result value")};
        return nullptr;
    }
    if (value.of.result.is_ok)
        return value.of.result.val;
    PluginError failure{PluginError::Kind::Plugin, {}, {}};
    const Val *payload = value.of.result.val;
    const Val *code = payload ? field(*payload, "code") : nullptr;
    if (code && code->kind == WASMTIME_COMPONENT_ENUM)
        failure.code = toString(code->of.enumeration);
    if (payload)
        readStringField(*payload, "message", failure.message);
    *error = failure;
    return nullptr;
}

bool readPassword(const Val &record, ImportedPassword &out) {
    if (!readStringField(record, "display-name", out.displayName) || !readStringField(record, "user-name", out.userName) ||
        !readStringField(record, "email", out.email) || !readStringField(record, "website", out.website) ||
        !readStringField(record, "password", out.password) || !readStringField(record, "notes", out.notes) ||
        !readStringList(field(record, "tags"), out.tags))
        return false;
    const Val *totp = field(record, "totp");
    if (!totp || totp->kind != WASMTIME_COMPONENT_OPTION)
        return false;
    if (const Val *value = totp->of.option) {
        const Val *digits = field(*value, "digits");
        const Val *interval = field(*value, "interval");
        const Val *algorithm = field(*value, "algorithm");
        if (!readStringField(*value, "secret", out.totpSecret) || !digits || digits->kind != WASMTIME_COMPONENT_U8 ||
            !interval || interval->kind != WASMTIME_COMPONENT_U32 || !algorithm ||
            algorithm->kind != WASMTIME_COMPONENT_ENUM)
            return false;
        out.hasTotp = true;
        out.totpDigits = digits->of.u8;
        out.totpInterval = int(interval->of.u32);
        out.totpAlgorithm = toString(algorithm->of.enumeration);
    }
    return true;
}

} // namespace

ImporterPlugin::ImporterPlugin(std::unique_ptr<Private> d) : d(std::move(d)) {}
ImporterPlugin::~ImporterPlugin() = default;

const PluginInspection &ImporterPlugin::inspection() const { return d->inspection; }

std::unique_ptr<ImporterPlugin> ImporterPlugin::load(const QByteArray &componentBytes,
                                                     const QStringList &approvedPermissions, PluginHost host,
                                                     PluginLimits limits, PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    *error = {};

    auto d = std::make_unique<Private>();
    d->host = std::move(host);
    d->limits = limits;

    // 1. Inspect before anything of the plugin can run.
    d->inspection = PluginInspector::inspect(componentBytes);
    if (!d->inspection.accepted()) {
        *error = {PluginError::Kind::Rejected, {}, d->inspection.rejected.join(QStringLiteral("; "))};
        return nullptr;
    }
    if (!d->inspection.types.contains(QStringLiteral("importer"))) {
        *error = {PluginError::Kind::Rejected, {}, QStringLiteral("the plugin is not an importer")};
        return nullptr;
    }

    wasm_engine_t *engine = PluginEngine::instance().engine();
    if (wasmtime_error_t *failure =
            wasmtime_component_new(engine, reinterpret_cast<const uint8_t *>(componentBytes.constData()),
                                   size_t(componentBytes.size()), &d->component)) {
        *error = {PluginError::Kind::Rejected, {}, takeErrorText(failure)};
        return nullptr;
    }

    // 2. A store of its own with a memory cap and a deadline check.
    d->store = wasmtime_store_new(engine, nullptr, nullptr);
    d->context = wasmtime_store_context(d->store);
    wasmtime_store_limiter(d->store, limits.maxMemoryBytes, -1, -1, -1, -1);
    wasmtime_store_epoch_deadline_callback(d->store, &Private::onEpoch, d.get(), nullptr);

    // 3. Explicit linker: neutral interfaces plus the approved permissions, nothing else.
    d->linker = wasmtime_component_linker_new(engine);
    QString linkError = d->defineInterface(TypesInterface, {});
    if (linkError.isEmpty())
        linkError = d->defineInterface(LogInterface, {{"log", &Private::hostLog}});
    if (linkError.isEmpty() && approvedPermissions.contains(QStringLiteral("pick-file")))
        linkError = d->defineInterface(FilePickerInterface, {{"pick-file", &Private::hostPickFile}});
    if (!linkError.isEmpty()) {
        *error = {PluginError::Kind::Host, {}, linkError};
        return nullptr;
    }

    // 4. Instantiate. An import that is not defined above makes this fail.
    const bool instantiated = d->guarded(error, [&] {
        return wasmtime_component_linker_instantiate(d->linker, d->context, d->component, &d->instance);
    });
    if (!instantiated) {
        if (error->kind == PluginError::Kind::Trap)
            error->kind = PluginError::Kind::Instantiate;
        return nullptr;
    }
    if (!d->lookup("sources", &d->sourcesFunc) || !d->lookup("import-passwords", &d->importFunc)) {
        *error = {PluginError::Kind::Host, {}, QStringLiteral("importer functions not found")};
        return nullptr;
    }
    return std::unique_ptr<ImporterPlugin>(new ImporterPlugin(std::move(d)));
}

QList<ImportSource> ImporterPlugin::sources(PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    *error = {};

    QList<ImportSource> sources;
    Val result{};
    if (!d->call(d->sourcesFunc, nullptr, 0, &result, error))
        return sources;
    bool valid = result.kind == WASMTIME_COMPONENT_LIST;
    for (size_t i = 0; valid && i < result.of.list.size; ++i) {
        const Val &record = result.of.list.data[i];
        ImportSource source;
        valid = readStringField(record, "id", source.id) && readStringField(record, "name", source.name) &&
                readStringField(record, "description", source.description);
        sources << source;
    }
    wasmtime_component_val_delete(&result);
    if (!valid) {
        *error = {PluginError::Kind::Host, {}, QStringLiteral("sources returned an unexpected value")};
        return {};
    }
    return sources;
}

QList<ImportedPassword> ImporterPlugin::importPasswords(const QString &sourceId, PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    *error = {};

    QList<ImportedPassword> passwords;
    Val argument{};
    setString(argument, sourceId.toUtf8());
    Val result{};
    const bool called = d->call(d->importFunc, &argument, 1, &result, error);
    wasmtime_component_val_delete(&argument);
    if (!called)
        return passwords;

    if (const Val *list = unwrapResult(result, error)) {
        bool valid = list->kind == WASMTIME_COMPONENT_LIST;
        for (size_t i = 0; valid && i < list->of.list.size; ++i) {
            ImportedPassword password;
            valid = readPassword(list->of.list.data[i], password);
            passwords << password;
        }
        if (!valid) {
            *error = {PluginError::Kind::Host, {}, QStringLiteral("import-passwords returned an unexpected value")};
            passwords.clear();
        }
    }
    wasmtime_component_val_delete(&result);
    return passwords;
}

} // namespace EasePass::Plugin
