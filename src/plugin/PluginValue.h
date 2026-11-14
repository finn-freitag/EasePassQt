#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace EasePass::Plugin {

/**
 * A value that crosses the plugin boundary (Component Model value: WIT types).
 * Host functions and adapters work with this type and never see Wasmtime structures.
 *
 * `list<u8>` is kept as a QByteArray (Kind::Bytes) because the generic list form costs
 * 32 bytes per element.
 */
class Value {
public:
    enum class Kind {
        Unit, ///< no value (functions without result, `result<_, E>` payload)
        Bool,
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        S64,
        U64,
        F32,
        F64,
        Char,
        String,
        Bytes,
        List,
        Record,
        Tuple,
        Enum,
        Option,
        Result,
    };
    using Field = QPair<QString, Value>;

    Value() {}

    static Value boolean(bool value);
    static Value u8(quint8 value) { return integer(Kind::U8, value); }
    static Value u16(quint16 value) { return integer(Kind::U16, value); }
    static Value u32(quint32 value) { return integer(Kind::U32, value); }
    static Value u64(quint64 value) { return integer(Kind::U64, value); }
    static Value integer(Kind kind, quint64 bits);
    static Value real(Kind kind, double value);
    static Value string(const QString &text);
    static Value bytes(const QByteArray &data);
    static Value list(const QList<Value> &items);
    static Value stringList(const QStringList &items);
    static Value tuple(const QList<Value> &items);
    static Value record(const QList<Field> &fields);
    static Value enumCase(const QString &name);
    static Value some(const Value &payload);
    static Value none();
    static Value ok(); ///< `ok` without payload (`result<_, E>`)
    static Value ok(const Value &payload);
    static Value err(const Value &payload);
    /// `err(error { code, message })` of the plugin interface (types.wit).
    static Value error(const QString &code, const QString &message);

    Kind kind() const { return m_kind; }
    bool isUnit() const { return m_kind == Kind::Unit; }

    bool toBool() const { return m_number != 0; }
    quint64 toU64() const { return m_number; }
    qint64 toS64() const { return qint64(m_number); }
    double toDouble() const { return m_real; }
    /// Text of a string, or the case name of an enum.
    QString toString() const { return m_text; }
    /// Content of a `list<u8>` (also for an empty generic list).
    QByteArray toBytes() const;
    /// Elements of a list or tuple (a byte list is expanded to u8 values).
    QList<Value> items() const;
    QStringList toStringList() const;
    const QList<Field> &fields() const { return m_fields; }
    /// Field of a record; a Unit value if it does not exist.
    const Value &field(const QString &name) const;

    bool isSome() const { return m_kind == Kind::Option && !m_items.isEmpty(); }
    bool isOk() const { return m_kind == Kind::Result && m_number != 0; }
    /// Payload of `some`, `ok` or `err`; Unit if there is none.
    const Value &payload() const;

private:
    Kind m_kind = Kind::Unit;
    quint64 m_number = 0;
    double m_real = 0;
    QString m_text;
    QByteArray m_bytes;
    QList<Value> m_items;
    QList<Field> m_fields;
};

} // namespace EasePass::Plugin
