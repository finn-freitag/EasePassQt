#include "PluginInstance.h"

#include "PluginEngine.h"

#include <chrono>
#include <cstring>
#include <vector>
#include <wasmtime.h>

namespace EasePass::Plugin {

namespace {

using Clock = std::chrono::steady_clock;
using WasmValue = wasmtime_component_val_t;

constexpr char TypesInterface[] = "easepass:plugin/types@1.0.0";

QString takeErrorText(wasmtime_error_t *error) {
    wasm_name_t message;
    wasmtime_error_message(error, &message);
    const QString text = QString::fromUtf8(message.data, qsizetype(message.size));
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return text;
}

QString toQString(const wasm_name_t &name) { return QString::fromUtf8(name.data, qsizetype(name.size)); }

void setName(wasm_name_t &out, const QString &text) {
    const QByteArray utf8 = text.toUtf8();
    wasm_byte_vec_new(&out, size_t(utf8.size()), utf8.constData());
}

Value fromWasm(const WasmValue &value) {
    switch (value.kind) {
    case WASMTIME_COMPONENT_BOOL:
        return Value::boolean(value.of.boolean);
    case WASMTIME_COMPONENT_S8:
        return Value::integer(Value::Kind::S8, quint64(qint64(value.of.s8)));
    case WASMTIME_COMPONENT_U8:
        return Value::u8(value.of.u8);
    case WASMTIME_COMPONENT_S16:
        return Value::integer(Value::Kind::S16, quint64(qint64(value.of.s16)));
    case WASMTIME_COMPONENT_U16:
        return Value::u16(value.of.u16);
    case WASMTIME_COMPONENT_S32:
        return Value::integer(Value::Kind::S32, quint64(qint64(value.of.s32)));
    case WASMTIME_COMPONENT_U32:
        return Value::u32(value.of.u32);
    case WASMTIME_COMPONENT_S64:
        return Value::integer(Value::Kind::S64, quint64(value.of.s64));
    case WASMTIME_COMPONENT_U64:
        return Value::u64(value.of.u64);
    case WASMTIME_COMPONENT_F32:
        return Value::real(Value::Kind::F32, double(value.of.f32));
    case WASMTIME_COMPONENT_F64:
        return Value::real(Value::Kind::F64, value.of.f64);
    case WASMTIME_COMPONENT_CHAR:
        return Value::integer(Value::Kind::Char, value.of.character);
    case WASMTIME_COMPONENT_STRING:
        return Value::string(toQString(value.of.string));
    case WASMTIME_COMPONENT_ENUM:
        return Value::enumCase(toQString(value.of.enumeration));
    case WASMTIME_COMPONENT_LIST: {
        const wasmtime_component_vallist_t &list = value.of.list;
        if (list.size > 0 && list.data[0].kind == WASMTIME_COMPONENT_U8) {
            QByteArray bytes(qsizetype(list.size), Qt::Uninitialized);
            for (size_t i = 0; i < list.size; ++i)
                bytes[qsizetype(i)] = char(list.data[i].of.u8);
            return Value::bytes(bytes);
        }
        QList<Value> items;
        items.reserve(qsizetype(list.size));
        for (size_t i = 0; i < list.size; ++i)
            items << fromWasm(list.data[i]);
        return Value::list(items);
    }
    case WASMTIME_COMPONENT_TUPLE: {
        QList<Value> items;
        for (size_t i = 0; i < value.of.tuple.size; ++i)
            items << fromWasm(value.of.tuple.data[i]);
        return Value::tuple(items);
    }
    case WASMTIME_COMPONENT_RECORD: {
        QList<Value::Field> fields;
        for (size_t i = 0; i < value.of.record.size; ++i) {
            const wasmtime_component_valrecord_entry_t &entry = value.of.record.data[i];
            fields << Value::Field(toQString(entry.name), fromWasm(entry.val));
        }
        return Value::record(fields);
    }
    case WASMTIME_COMPONENT_OPTION:
        return value.of.option ? Value::some(fromWasm(*value.of.option)) : Value::none();
    case WASMTIME_COMPONENT_RESULT: {
        const Value payload = value.of.result.val ? fromWasm(*value.of.result.val) : Value();
        return value.of.result.is_ok ? Value::ok(payload) : Value::err(payload);
    }
    default:
        // variant, flags, resource, map: not used by easepass:plugin@1
        return {};
    }
}

/// Fills `out` (uninitialised). Ownership of everything allocated goes to whoever owns `out`.
void toWasm(const Value &value, WasmValue *out) {
    std::memset(out, 0, sizeof(*out));
    switch (value.kind()) {
    case Value::Kind::Unit:
    case Value::Kind::Bool:
        out->kind = WASMTIME_COMPONENT_BOOL;
        out->of.boolean = value.toBool();
        break;
    case Value::Kind::S8:
        out->kind = WASMTIME_COMPONENT_S8;
        out->of.s8 = int8_t(value.toS64());
        break;
    case Value::Kind::U8:
        out->kind = WASMTIME_COMPONENT_U8;
        out->of.u8 = uint8_t(value.toU64());
        break;
    case Value::Kind::S16:
        out->kind = WASMTIME_COMPONENT_S16;
        out->of.s16 = int16_t(value.toS64());
        break;
    case Value::Kind::U16:
        out->kind = WASMTIME_COMPONENT_U16;
        out->of.u16 = uint16_t(value.toU64());
        break;
    case Value::Kind::S32:
        out->kind = WASMTIME_COMPONENT_S32;
        out->of.s32 = int32_t(value.toS64());
        break;
    case Value::Kind::U32:
        out->kind = WASMTIME_COMPONENT_U32;
        out->of.u32 = uint32_t(value.toU64());
        break;
    case Value::Kind::S64:
        out->kind = WASMTIME_COMPONENT_S64;
        out->of.s64 = value.toS64();
        break;
    case Value::Kind::U64:
        out->kind = WASMTIME_COMPONENT_U64;
        out->of.u64 = value.toU64();
        break;
    case Value::Kind::F32:
        out->kind = WASMTIME_COMPONENT_F32;
        out->of.f32 = float(value.toDouble());
        break;
    case Value::Kind::F64:
        out->kind = WASMTIME_COMPONENT_F64;
        out->of.f64 = value.toDouble();
        break;
    case Value::Kind::Char:
        out->kind = WASMTIME_COMPONENT_CHAR;
        out->of.character = uint32_t(value.toU64());
        break;
    case Value::Kind::String:
        out->kind = WASMTIME_COMPONENT_STRING;
        setName(out->of.string, value.toString());
        break;
    case Value::Kind::Enum:
        out->kind = WASMTIME_COMPONENT_ENUM;
        setName(out->of.enumeration, value.toString());
        break;
    case Value::Kind::Bytes: {
        const QByteArray bytes = value.toBytes();
        out->kind = WASMTIME_COMPONENT_LIST;
        wasmtime_component_vallist_new_uninit(&out->of.list, size_t(bytes.size()));
        for (qsizetype i = 0; i < bytes.size(); ++i) {
            std::memset(&out->of.list.data[i], 0, sizeof(WasmValue));
            out->of.list.data[i].kind = WASMTIME_COMPONENT_U8;
            out->of.list.data[i].of.u8 = uint8_t(bytes.at(i));
        }
        break;
    }
    case Value::Kind::List:
    case Value::Kind::Tuple: {
        const QList<Value> items = value.items();
        const bool isList = value.kind() == Value::Kind::List;
        out->kind = isList ? WASMTIME_COMPONENT_LIST : WASMTIME_COMPONENT_TUPLE;
        if (isList)
            wasmtime_component_vallist_new_uninit(&out->of.list, size_t(items.size()));
        else
            wasmtime_component_valtuple_new_uninit(&out->of.tuple, size_t(items.size()));
        WasmValue *data = isList ? out->of.list.data : out->of.tuple.data;
        for (qsizetype i = 0; i < items.size(); ++i)
            toWasm(items.at(i), &data[i]);
        break;
    }
    case Value::Kind::Record: {
        const QList<Value::Field> &fields = value.fields();
        out->kind = WASMTIME_COMPONENT_RECORD;
        wasmtime_component_valrecord_new_uninit(&out->of.record, size_t(fields.size()));
        for (qsizetype i = 0; i < fields.size(); ++i) {
            setName(out->of.record.data[i].name, fields.at(i).first);
            toWasm(fields.at(i).second, &out->of.record.data[i].val);
        }
        break;
    }
    case Value::Kind::Option:
        out->kind = WASMTIME_COMPONENT_OPTION;
        if (value.isSome()) {
            WasmValue payload;
            toWasm(value.payload(), &payload);
            out->of.option = wasmtime_component_val_new(&payload);
        }
        break;
    case Value::Kind::Result:
        out->kind = WASMTIME_COMPONENT_RESULT;
        out->of.result.is_ok = value.isOk();
        if (!value.payload().isUnit()) {
            WasmValue payload;
            toWasm(value.payload(), &payload);
            out->of.result.val = wasmtime_component_val_new(&payload);
        }
        break;
    }
}

} // namespace

Value PluginError::unwrap(const Value &result, PluginError *error) {
    if (result.kind() != Value::Kind::Result) {
        *error = {Kind::Host, {}, QStringLiteral("the plugin returned an unexpected value")};
        return {};
    }
    if (result.isOk())
        return result.payload();
    const Value &payload = result.payload();
    *error = {Kind::Plugin, payload.field(QStringLiteral("code")).toString(),
              payload.field(QStringLiteral("message")).toString()};
    return {};
}

struct PluginInstance::Private {
    struct Binding {
        Private *owner;
        HostFunction function;
    };

    PluginInspection inspection;
    PluginLimits limits;
    std::vector<std::unique_ptr<Binding>> bindings;

    wasmtime_component_t *component = nullptr;
    wasmtime_store_t *store = nullptr;
    wasmtime_context_t *context = nullptr;
    wasmtime_component_linker_t *linker = nullptr;
    wasmtime_component_instance_t instance{};
    QHash<QString, wasmtime_component_func_t> functions;

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
        const bool cancelled = self->limits.cancel && self->limits.cancel->load();
        if (cancelled || Clock::now() >= self->deadline) {
            self->timedOut = true;
            return wasmtime_error_new("plugin call timed out");
        }
        *delta = 1;
        *kind = WASMTIME_UPDATE_DEADLINE_CONTINUE;
        return nullptr;
    }

    static wasmtime_error_t *hostCall(void *data, wasmtime_context_t *, const wasmtime_component_func_type_t *,
                                      WasmValue *arguments, size_t argumentCount, WasmValue *results,
                                      size_t resultCount) {
        auto *binding = static_cast<Binding *>(data);
        QList<Value> values;
        values.reserve(qsizetype(argumentCount));
        for (size_t i = 0; i < argumentCount; ++i)
            values << fromWasm(arguments[i]);

        const Clock::time_point before = Clock::now();
        Value result;
        try {
            result = binding->function.function(values);
        } catch (const HostTrap &trap) {
            return wasmtime_error_new(trap.what());
        } catch (const std::exception &failure) {
            return wasmtime_error_new(failure.what());
        }
        if (binding->function.interactive)
            binding->owner->deadline += Clock::now() - before;
        if (resultCount == 1)
            toWasm(result, &results[0]);
        return nullptr;
    }

    QString defineInterface(const HostInterface &interface) {
        const QByteArray name = interface.name.toUtf8();
        wasmtime_component_linker_instance_t *root = wasmtime_component_linker_root(linker);
        wasmtime_component_linker_instance_t *instance = nullptr;
        wasmtime_error_t *error =
            wasmtime_component_linker_instance_add_instance(root, name.constData(), size_t(name.size()), &instance);
        for (auto it = interface.functions.cbegin(); !error && it != interface.functions.cend(); ++it) {
            bindings.push_back(std::make_unique<Binding>(Binding{this, it.value()}));
            const QByteArray functionName = it.key().toUtf8();
            error = wasmtime_component_linker_instance_add_func(instance, functionName.constData(),
                                                                size_t(functionName.size()), &Private::hostCall,
                                                                bindings.back().get(), nullptr);
        }
        if (instance)
            wasmtime_component_linker_instance_delete(instance);
        wasmtime_component_linker_instance_delete(root);
        return error ? takeErrorText(error) : QString();
    }

    bool lookup(const QString &interface, const QString &function, wasmtime_component_func_t *out) {
        const QString key = interface + QLatin1Char('#') + function;
        const auto cached = functions.constFind(key);
        if (cached != functions.cend()) {
            *out = cached.value();
            return true;
        }
        // The export carries a version: find the one whose base name matches.
        QByteArray exportName;
        for (const QString &exported : inspection.exports) {
            if (exported.section(QLatin1Char('@'), 0, 0) == interface)
                exportName = exported.toUtf8();
        }
        if (exportName.isEmpty())
            return false;
        wasmtime_component_export_index_t *interfaceIndex =
            wasmtime_component_get_export_index(component, nullptr, exportName.constData(), size_t(exportName.size()));
        if (!interfaceIndex)
            return false;
        const QByteArray functionName = function.toUtf8();
        wasmtime_component_export_index_t *index = wasmtime_component_get_export_index(
            component, interfaceIndex, functionName.constData(), size_t(functionName.size()));
        wasmtime_component_export_index_delete(interfaceIndex);
        if (!index)
            return false;
        const bool found = wasmtime_component_instance_get_func(&instance, context, index, out);
        wasmtime_component_export_index_delete(index);
        if (found)
            functions.insert(key, *out);
        return found;
    }

    /// Runs `body` (which enters the plugin) with the timeout armed and classifies failures.
    template <typename Body> bool guarded(int timeoutMs, PluginError *error, Body body) {
        if (unusable) {
            *error = {PluginError::Kind::Unusable, {},
                      QStringLiteral("the plugin instance was stopped after an earlier failure")};
            return false;
        }
        PluginEngine::CallGuard ticking;
        timedOut = false;
        deadline = Clock::now() + std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : limits.callTimeoutMs);
        wasmtime_context_set_epoch_deadline(context, 1);
        if (wasmtime_error_t *failure = body()) {
            // After a trap or interruption the instance state is undefined: never reuse it.
            unusable = true;
            *error = {timedOut ? PluginError::Kind::Timeout : PluginError::Kind::Trap, {}, takeErrorText(failure)};
            return false;
        }
        return true;
    }
};

PluginInstance::PluginInstance(std::unique_ptr<Private> d) : d(std::move(d)) {}
PluginInstance::~PluginInstance() = default;

const PluginInspection &PluginInstance::inspection() const { return d->inspection; }
bool PluginInstance::usable() const { return !d->unusable; }

std::unique_ptr<PluginInstance> PluginInstance::create(const QByteArray &componentBytes,
                                                       const QList<HostInterface> &hostInterfaces,
                                                       PluginLimits limits, PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    *error = {};

    auto d = std::make_unique<Private>();
    d->limits = limits;

    // 1. Inspect before anything of the plugin can run.
    d->inspection = PluginInspector::inspect(componentBytes);
    if (!d->inspection.accepted()) {
        *error = {PluginError::Kind::Rejected, {}, d->inspection.rejected.join(QStringLiteral("; "))};
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

    // 3. Explicit linker: the type-only interface plus exactly what the caller passes.
    d->linker = wasmtime_component_linker_new(engine);
    QString linkError = d->defineInterface({QString::fromLatin1(TypesInterface), {}});
    for (const HostInterface &interface : hostInterfaces) {
        if (!linkError.isEmpty())
            break;
        linkError = d->defineInterface(interface);
    }
    if (!linkError.isEmpty()) {
        *error = {PluginError::Kind::Host, {}, linkError};
        return nullptr;
    }

    // 4. Instantiate. An import that is not defined above makes this fail.
    const bool instantiated = d->guarded(0, error, [&] {
        return wasmtime_component_linker_instantiate(d->linker, d->context, d->component, &d->instance);
    });
    if (!instantiated) {
        if (error->kind == PluginError::Kind::Trap)
            error->kind = PluginError::Kind::Instantiate;
        return nullptr;
    }
    return std::unique_ptr<PluginInstance>(new PluginInstance(std::move(d)));
}

Value PluginInstance::call(const QString &interface, const QString &function, const QList<Value> &arguments,
                           int timeoutMs, PluginError *error) {
    PluginError ignored;
    if (!error)
        error = &ignored;
    *error = {};

    if (d->unusable) {
        *error = {PluginError::Kind::Unusable, {},
                  QStringLiteral("the plugin instance was stopped after an earlier failure")};
        return {};
    }
    wasmtime_component_func_t target{};
    if (!d->lookup(interface, function, &target)) {
        *error = {PluginError::Kind::Host, {},
                  QStringLiteral("the plugin does not export %1.%2").arg(interface, function)};
        return {};
    }

    // Ask the function itself whether it returns something.
    wasmtime_component_func_type_t *type = wasmtime_component_func_type(&target, d->context);
    wasmtime_component_valtype_t resultType;
    const bool hasResult = wasmtime_component_func_type_result(type, &resultType);
    if (hasResult)
        wasmtime_component_valtype_delete(&resultType);
    const size_t parameterCount = wasmtime_component_func_type_param_count(type);
    wasmtime_component_func_type_delete(type);
    if (parameterCount != size_t(arguments.size())) {
        *error = {PluginError::Kind::Host, {},
                  QStringLiteral("%1.%2 takes %3 arguments").arg(interface, function).arg(parameterCount)};
        return {};
    }

    std::vector<WasmValue> wasmArguments(size_t(arguments.size()));
    for (qsizetype i = 0; i < arguments.size(); ++i)
        toWasm(arguments.at(i), &wasmArguments[size_t(i)]);
    WasmValue wasmResult{};

    const bool called = d->guarded(timeoutMs, error, [&]() -> wasmtime_error_t * {
        if (wasmtime_error_t *failure =
                wasmtime_component_func_call(&target, d->context, wasmArguments.data(), wasmArguments.size(),
                                             hasResult ? &wasmResult : nullptr, hasResult ? 1 : 0))
            return failure;
        return wasmtime_component_func_post_return(&target, d->context);
    });
    for (WasmValue &argument : wasmArguments)
        wasmtime_component_val_delete(&argument);

    Value result;
    if (called && hasResult)
        result = fromWasm(wasmResult);
    if (hasResult)
        wasmtime_component_val_delete(&wasmResult);
    return result;
}

} // namespace EasePass::Plugin
