#include "PluginHostFunctions.h"

#include "IClock.h"

#include <QElapsedTimer>
#include <QRandomGenerator>

namespace EasePass::Plugin {

namespace {

constexpr int MaxLogBytes = 4096;
constexpr int LogBurst = 200;          // messages ...
constexpr qint64 LogWindowMs = 10'000; // ... per window, the rest is dropped

Value fail(const std::optional<HostError> &error) { return Value::error(error->code, error->message); }
Value fail(const char *code, const QString &message) { return Value::error(QLatin1String(code), message); }

QString name(const char *interface) { return QStringLiteral("easepass:plugin/%1@1.0.0").arg(QLatin1String(interface)); }

HostInterface logInterface(const HostServices &services) {
    struct Throttle {
        QElapsedTimer window;
        int count = 0;
    };
    auto throttle = std::make_shared<Throttle>();
    const auto sink = services.log;

    HostInterface interface{name("log"), {}};
    interface.functions.insert(QStringLiteral("log"), {[sink, throttle](const QList<Value> &arguments) {
        if (!sink || arguments.size() != 2)
            return Value();
        if (!throttle->window.isValid() || throttle->window.elapsed() > LogWindowMs) {
            throttle->window.start();
            throttle->count = 0;
        }
        if (++throttle->count > LogBurst) {
            if (throttle->count == LogBurst + 1)
                sink(QStringLiteral("warning"), QStringLiteral("the plugin logs too much; messages are dropped"));
            return Value();
        }
        static const QStringList levels = {QStringLiteral("debug"), QStringLiteral("info"), QStringLiteral("warning"),
                                           QStringLiteral("error")};
        const QString level = arguments[0].toString();
        sink(levels.contains(level) ? level : QStringLiteral("info"), sanitizeLogMessage(arguments[1].toString()));
        return Value();
    }});
    return interface;
}

HostInterface randomInterface() {
    HostInterface interface{QStringLiteral("wasi:random/random@0.2.0"), {}};
    interface.functions.insert(QStringLiteral("get-random-bytes"), {[](const QList<Value> &arguments) {
        const quint64 length = arguments.value(0).toU64();
        if (length > 1024 * 1024)
            throw HostTrap("get-random-bytes: at most 1 MiB per call");
        QByteArray bytes(qsizetype(length), Qt::Uninitialized);
        // QRandomGenerator::system() is the operating system's cryptographic generator.
        for (qsizetype i = 0; i < bytes.size(); i += 4) {
            const quint32 word = QRandomGenerator::system()->generate();
            for (qsizetype j = 0; j < 4 && i + j < bytes.size(); ++j)
                bytes[i + j] = char(word >> (8 * j));
        }
        return Value::bytes(bytes);
    }});
    interface.functions.insert(QStringLiteral("get-random-u64"), {[](const QList<Value> &) {
        return Value::u64(QRandomGenerator::system()->generate64());
    }});
    return interface;
}

HostInterface clockInterface(const Core::IClock *clock) {
    const auto datetime = [](quint64 seconds, quint32 nanoseconds) {
        return Value::record({{QStringLiteral("seconds"), Value::u64(seconds)},
                              {QStringLiteral("nanoseconds"), Value::u32(nanoseconds)}});
    };
    HostInterface interface{QStringLiteral("wasi:clocks/wall-clock@0.2.0"), {}};
    interface.functions.insert(QStringLiteral("now"), {[clock, datetime](const QList<Value> &) {
        if (clock)
            return datetime(quint64(clock->currentSecsSinceEpoch()), 0);
        const qint64 ms = QDateTime::currentMSecsSinceEpoch();
        return datetime(quint64(ms / 1000), quint32(ms % 1000) * 1'000'000);
    }});
    interface.functions.insert(QStringLiteral("resolution"),
                               {[datetime](const QList<Value> &) { return datetime(0, 1'000'000); }});
    return interface;
}

HostInterface storageInterface(const std::shared_ptr<PluginStorage> &storage) {
    HostInterface interface{name("storage"), {}};
    const auto add = [&](const char *function, std::function<Value(PluginStorage &, const QList<Value> &)> body) {
        interface.functions.insert(QLatin1String(function), {[storage, body](const QList<Value> &arguments) {
            if (!storage)
                return fail("unsupported", QStringLiteral("storage is not available"));
            return body(*storage, arguments);
        }});
    };
    add("get-string", [](PluginStorage &s, const QList<Value> &a) {
        std::optional<QString> value;
        if (const auto error = s.getString(a.value(0).toString(), &value))
            return fail(error);
        return Value::ok(value ? Value::some(Value::string(*value)) : Value::none());
    });
    add("set-string", [](PluginStorage &s, const QList<Value> &a) {
        const auto error = s.setString(a.value(0).toString(), a.value(1).toString());
        return error ? fail(error) : Value::ok();
    });
    add("delete-string", [](PluginStorage &s, const QList<Value> &a) {
        const auto error = s.deleteString(a.value(0).toString());
        return error ? fail(error) : Value::ok();
    });
    add("list-keys", [](PluginStorage &s, const QList<Value> &) {
        QStringList keys;
        if (const auto error = s.listKeys(&keys))
            return fail(error);
        return Value::ok(Value::stringList(keys));
    });
    add("read-file", [](PluginStorage &s, const QList<Value> &a) {
        std::optional<QByteArray> data;
        if (const auto error = s.readFile(a.value(0).toString(), &data))
            return fail(error);
        return Value::ok(data ? Value::some(Value::bytes(*data)) : Value::none());
    });
    add("write-file", [](PluginStorage &s, const QList<Value> &a) {
        const auto error = s.writeFile(a.value(0).toString(), a.value(1).toBytes());
        return error ? fail(error) : Value::ok();
    });
    add("delete-file", [](PluginStorage &s, const QList<Value> &a) {
        const auto error = s.deleteFile(a.value(0).toString());
        return error ? fail(error) : Value::ok();
    });
    add("list-files", [](PluginStorage &s, const QList<Value> &) {
        QStringList names;
        if (const auto error = s.listFiles(&names))
            return fail(error);
        return Value::ok(Value::stringList(names));
    });
    return interface;
}

HostInterface filePickerInterface(const HostServices &services) {
    const auto pick = services.pickFile;
    HostInterface interface{name("file-picker"), {}};
    HostFunction function;
    function.interactive = true; // the user decides how long the dialog stays open
    function.function = [pick](const QList<Value> &arguments) {
        if (!pick)
            return fail("unsupported", QStringLiteral("no file dialog is available"));
        const Value options = arguments.value(0);
        const Value &title = options.field(QStringLiteral("title"));
        QStringList extensions;
        for (const QString &extension : options.field(QStringLiteral("extensions")).toStringList()) {
            // Extensions end up in a dialog filter: keep plain ones only.
            QString clean = extension.trimmed();
            while (clean.startsWith(QLatin1Char('.')) || clean.startsWith(QLatin1Char('*')))
                clean.remove(0, 1);
            const bool plain = !clean.isEmpty() && clean.size() <= 16 &&
                               std::all_of(clean.cbegin(), clean.cend(), [](QChar c) { return c.isLetterOrNumber(); });
            if (plain)
                extensions << clean.toLower();
        }
        const std::optional<PickedFile> file =
            pick(title.isSome() ? sanitizeLogMessage(title.payload().toString()).left(120) : QString(), extensions);
        if (!file)
            return Value::ok(Value::none());
        if (file->data.size() > PluginLimits::MaxByteListSize)
            return fail("too-large", QStringLiteral("the file is larger than 8 MiB"));
        return Value::ok(Value::some(Value::record(
            {{QStringLiteral("name"), Value::string(file->name)}, {QStringLiteral("data"), Value::bytes(file->data)}})));
    };
    interface.functions.insert(QStringLiteral("pick-file"), function);
    return interface;
}

HostInterface networkInterface(const std::shared_ptr<PluginNetwork> &network) {
    HostInterface interface{name("network"), {}};
    const auto add = [&](const char *function, std::function<Value(PluginNetwork &, const QList<Value> &)> body) {
        interface.functions.insert(QLatin1String(function), {[network, body](const QList<Value> &arguments) {
            if (!network)
                return fail("unsupported", QStringLiteral("networking is not available"));
            return body(*network, arguments);
        }});
    };
    const auto timeout = [](const Value &option) {
        return option.isSome() ? std::optional<quint32>(quint32(option.payload().toU64())) : std::nullopt;
    };
    add("http-request", [timeout](PluginNetwork &n, const QList<Value> &a) {
        const Value request = a.value(0);
        PluginNetwork::HttpRequest httpRequest;
        httpRequest.method = request.field(QStringLiteral("method")).toString();
        httpRequest.url = request.field(QStringLiteral("url")).toString();
        for (const Value &header : request.field(QStringLiteral("headers")).items())
            httpRequest.headers << PluginNetwork::Header(header.field(QStringLiteral("name")).toString(),
                                                         header.field(QStringLiteral("value")).toString());
        const Value &body = request.field(QStringLiteral("body"));
        if (body.isSome())
            httpRequest.body = body.payload().toBytes();
        httpRequest.timeoutMs = timeout(request.field(QStringLiteral("timeout-ms")));

        PluginNetwork::HttpResponse response;
        if (const auto error = n.httpRequest(httpRequest, &response))
            return fail(error);
        QList<Value> headers;
        for (const PluginNetwork::Header &header : std::as_const(response.headers))
            headers << Value::record({{QStringLiteral("name"), Value::string(header.first)},
                                      {QStringLiteral("value"), Value::string(header.second)}});
        return Value::ok(Value::record({{QStringLiteral("status"), Value::u16(quint16(response.status))},
                                        {QStringLiteral("headers"), Value::list(headers)},
                                        {QStringLiteral("body"), Value::bytes(response.body)}}));
    });
    add("tcp-connect", [timeout](PluginNetwork &n, const QList<Value> &a) {
        const Value options = a.value(2);
        quint32 handle = 0;
        if (const auto error =
                n.tcpConnect(a.value(0).toString(), quint16(a.value(1).toU64()),
                             options.field(QStringLiteral("tls")).toBool(),
                             timeout(options.field(QStringLiteral("timeout-ms"))), &handle))
            return fail(error);
        return Value::ok(Value::u32(handle));
    });
    add("tcp-read", [](PluginNetwork &n, const QList<Value> &a) {
        QByteArray data;
        if (const auto error = n.tcpRead(quint32(a.value(0).toU64()), quint32(a.value(1).toU64()), &data))
            return fail(error);
        return Value::ok(Value::bytes(data));
    });
    add("tcp-write", [](PluginNetwork &n, const QList<Value> &a) {
        quint32 written = 0;
        if (const auto error = n.tcpWrite(quint32(a.value(0).toU64()), a.value(1).toBytes(), &written))
            return fail(error);
        return Value::ok(Value::u32(written));
    });
    add("tcp-start-tls", [](PluginNetwork &n, const QList<Value> &a) {
        const auto error = n.tcpStartTls(quint32(a.value(0).toU64()));
        return error ? fail(error) : Value::ok();
    });
    add("tcp-close", [](PluginNetwork &n, const QList<Value> &a) {
        n.tcpClose(quint32(a.value(0).toU64()));
        return Value();
    });
    return interface;
}

HostInterface leakCheckInterface(const HostServices &services) {
    const auto check = services.leakCheck;
    HostInterface interface{name("leak-check"), {}};
    interface.functions.insert(QStringLiteral("check-password"), {[check](const QList<Value> &arguments) {
        LeakCheckOutcome outcome;
        if (check)
            outcome = check(arguments.value(0).toString());
        static const QStringList statuses = {QStringLiteral("not-leaked"), QStringLiteral("leaked"),
                                             QStringLiteral("unknown")};
        if (!statuses.contains(outcome.status))
            outcome = {};
        return Value::ok(Value::record({{QStringLiteral("status"), Value::enumCase(outcome.status)},
                                        {QStringLiteral("count"), Value::u64(outcome.count)}}));
    }});
    return interface;
}

} // namespace

QString sanitizeLogMessage(const QString &message) {
    QString clean;
    clean.reserve(message.size());
    for (const QChar c : message) {
        if (c == QLatin1Char('\n') || c == QLatin1Char('\t'))
            clean += QLatin1Char(' ');
        else if (c.category() != QChar::Other_Control && c.category() != QChar::Other_Format &&
                 c.unicode() != 0x2028 && c.unicode() != 0x2029)
            clean += c;
    }
    QByteArray utf8 = clean.toUtf8();
    if (utf8.size() > MaxLogBytes) {
        utf8.truncate(MaxLogBytes);
        clean = QString::fromUtf8(utf8); // a cut multi-byte character becomes U+FFFD
        while (clean.endsWith(QChar(0xFFFD)))
            clean.chop(1);
    }
    return clean;
}

QList<HostInterface> buildHostInterfaces(const HostServices &services, const QStringList &approvedPermissions) {
    QList<HostInterface> interfaces = {logInterface(services), randomInterface(), clockInterface(services.clock)};
    if (approvedPermissions.contains(QStringLiteral("storage")))
        interfaces << storageInterface(services.storage);
    if (approvedPermissions.contains(QStringLiteral("pick-file")))
        interfaces << filePickerInterface(services);
    if (approvedPermissions.contains(QStringLiteral("network")))
        interfaces << networkInterface(services.network);
    if (approvedPermissions.contains(QStringLiteral("leak-check")))
        interfaces << leakCheckInterface(services);
    return interfaces;
}

} // namespace EasePass::Plugin
