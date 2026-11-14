#include "PluginValue.h"

namespace EasePass::Plugin {

namespace {
const Value &unitValue() {
    static const Value unit;
    return unit;
}
} // namespace

Value Value::boolean(bool value) { return integer(Kind::Bool, value ? 1 : 0); }

Value Value::integer(Kind kind, quint64 bits) {
    Value v;
    v.m_kind = kind;
    v.m_number = bits;
    return v;
}

Value Value::real(Kind kind, double value) {
    Value v;
    v.m_kind = kind;
    v.m_real = value;
    return v;
}

Value Value::string(const QString &text) {
    Value v;
    v.m_kind = Kind::String;
    v.m_text = text;
    return v;
}

Value Value::bytes(const QByteArray &data) {
    Value v;
    v.m_kind = Kind::Bytes;
    v.m_bytes = data;
    return v;
}

Value Value::list(const QList<Value> &items) {
    Value v;
    v.m_kind = Kind::List;
    v.m_items = items;
    return v;
}

Value Value::stringList(const QStringList &items) {
    QList<Value> values;
    values.reserve(items.size());
    for (const QString &item : items)
        values << string(item);
    return list(values);
}

Value Value::tuple(const QList<Value> &items) {
    Value v = list(items);
    v.m_kind = Kind::Tuple;
    return v;
}

Value Value::record(const QList<Field> &fields) {
    Value v;
    v.m_kind = Kind::Record;
    v.m_fields = fields;
    return v;
}

Value Value::enumCase(const QString &name) {
    Value v;
    v.m_kind = Kind::Enum;
    v.m_text = name;
    return v;
}

Value Value::some(const Value &payload) {
    Value v;
    v.m_kind = Kind::Option;
    v.m_items = {payload};
    return v;
}

Value Value::none() {
    Value v;
    v.m_kind = Kind::Option;
    return v;
}

Value Value::ok() { return ok(Value()); }

Value Value::ok(const Value &payload) {
    Value v;
    v.m_kind = Kind::Result;
    v.m_number = 1;
    if (!payload.isUnit())
        v.m_items = {payload};
    return v;
}

Value Value::err(const Value &payload) {
    Value v;
    v.m_kind = Kind::Result;
    if (!payload.isUnit())
        v.m_items = {payload};
    return v;
}

Value Value::error(const QString &code, const QString &message) {
    return err(record({{QStringLiteral("code"), enumCase(code)}, {QStringLiteral("message"), string(message)}}));
}

QByteArray Value::toBytes() const {
    if (m_kind == Kind::Bytes)
        return m_bytes;
    QByteArray data;
    data.reserve(m_items.size());
    for (const Value &item : m_items)
        data.append(char(item.m_number));
    return data;
}

QList<Value> Value::items() const {
    if (m_kind != Kind::Bytes)
        return m_items;
    QList<Value> values;
    values.reserve(m_bytes.size());
    for (char byte : m_bytes)
        values << u8(quint8(byte));
    return values;
}

QStringList Value::toStringList() const {
    QStringList list;
    for (const Value &item : m_items)
        list << item.m_text;
    return list;
}

const Value &Value::field(const QString &name) const {
    for (const Field &field : m_fields) {
        if (field.first == name)
            return field.second;
    }
    return unitValue();
}

const Value &Value::payload() const {
    if ((m_kind == Kind::Option || m_kind == Kind::Result) && !m_items.isEmpty())
        return m_items.first();
    return unitValue();
}

} // namespace EasePass::Plugin
