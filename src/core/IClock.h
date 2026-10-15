#pragma once

#include <QtGlobal>
#include <QDateTime>

namespace EasePass::Core {

class IClock {
public:
    virtual ~IClock() = default;
    virtual qint64 currentSecsSinceEpoch() const = 0;
};

class SystemClock : public IClock {
public:
    qint64 currentSecsSinceEpoch() const override { return QDateTime::currentSecsSinceEpoch(); }

    static SystemClock &instance() {
        static SystemClock s_instance;
        return s_instance;
    }
};

} // namespace EasePass::Core
