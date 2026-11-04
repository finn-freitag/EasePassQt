#include "AppController.h"

#include "DatabaseService.h"

namespace EasePass::App {

AppController::AppController(Core::DatabaseService *service, QObject *parent)
    : QObject(parent), m_service(service) {
    if (m_service) {
        connect(m_service, &Core::DatabaseService::opened, this, [this] {
            resetTo(Page::Passwords);
            emit databaseOpenChanged();
        });
        connect(m_service, &Core::DatabaseService::closed, this, [this] {
            resetTo(Page::Login);
            emit databaseOpenChanged();
        });
        if (m_service->isOpen())
            m_page = Page::Passwords;
    }
}

bool AppController::databaseOpen() const { return m_service && m_service->isOpen(); }

bool AppController::requiresOpenDatabase(Page page) {
    return page == Page::Passwords || page == Page::Database;
}

bool AppController::navigate(Page page) {
    if (requiresOpenDatabase(page) && !databaseOpen()) {
        emit navigationRejected(page);
        return false;
    }
    if (page == m_page)
        return true;
    m_history.append(m_page);
    m_page = page;
    emit pageChanged(m_page);
    return true;
}

bool AppController::goBack() {
    while (!m_history.isEmpty()) {
        const Page prev = m_history.takeLast();
        if (requiresOpenDatabase(prev) && !databaseOpen())
            continue;
        m_page = prev;
        emit pageChanged(m_page);
        return true;
    }
    return false;
}

void AppController::resetTo(Page page) {
    m_history.clear();
    m_page = page;
    emit pageChanged(m_page); // also covers canGoBack
}

} // namespace EasePass::App
