#include <QTest>
#include <QGuiApplication>
#include <QQuickItem>

#include "util/csdbuttonmodel.hpp"
#include "maininterface/mainctx.hpp"

// ---- link stubs for MainCtx symbols referenced by csdbuttonmodel.cpp ----
// (test never clicks; CSDButtonModel is constructed with a null MainCtx)
void MainCtx::requestInterfaceMaximized() {}
void MainCtx::requestInterfaceNormal() {}
void MainCtx::requestInterfaceMinimized() {}
QWindow *MainCtx::intfMainWindow() const { return nullptr; }

class TestQtCSDButtonModel : public QObject
{
    Q_OBJECT

    static CSDButton *buttonOfType(CSDButtonModel &model, CSDButton::ButtonType type)
    {
        for (CSDButton *b : model.windowCSDButtons())
            if (b->type() == type)
                return b;
        return nullptr;
    }

private slots:
    void init()
    {
        m_model = std::make_unique<CSDButtonModel>(nullptr, nullptr);
        m_root = new QQuickItem;                 // scene root (no window needed)
        m_root->setWidth(1000);
        m_root->setHeight(1000);
    }

    void cleanup()
    {
        delete m_root;                            // destroys children first
        m_model.reset();
    }

    void test_hitInsideVisibleItem()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *item = new QQuickItem(m_root);
        close->registerItem(item);
        close->setRect({960, 0, 40, 30});         // written by QML updateRect()

        QCOMPARE(m_model->buttonAt({980, 15}), close);   // center of button
        QCOMPARE(m_model->buttonAt({999, 29}), close);   // bottom-right corner
        // top 2px reserved for the resize handle
        QCOMPARE(m_model->buttonAt({980, 1}), nullptr);
        // outside the button
        QCOMPARE(m_model->buttonAt({950, 15}), nullptr);
    }

    void test_itemOwnVisibility()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *item = new QQuickItem(m_root);
        close->registerItem(item);
        close->setRect({960, 0, 40, 30});

        item->setVisible(false);                  // e.g. topbar faded out
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);

        item->setVisible(true);                   // e.g. topbar shown again
        QCOMPARE(m_model->buttonAt({980, 15}), close);

        item->setEnabled(false);                  // e.g. controls made inert
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);

        item->setEnabled(true);
        QCOMPARE(m_model->buttonAt({980, 15}), close);
    }

    void test_ancestorInvisibleOrDisabled()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *mid = new QQuickItem(m_root);       // e.g. the ToolBar topbar
        auto *item = new QQuickItem(mid);
        close->registerItem(item);
        close->setRect({960, 0, 40, 30});

        mid->setVisible(false);                   // invisible topbar (MenuTopbar)
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);
        mid->setVisible(true);

        mid->setEnabled(false);
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);
        mid->setEnabled(true);

        QCOMPARE(m_model->buttonAt({980, 15}), close);
    }

    void test_childUnderHiddenParent()
    {
        // decides whether dropping the ancestor walk is safe: Qt must
        // report the parent's hidden state through the child's own flags
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *hiddenParent = new QQuickItem(m_root);
        hiddenParent->setVisible(false);          // already hidden ...
        auto *child = new QQuickItem(hiddenParent); // ... when the child is created
        close->registerItem(child);
        close->setRect({960, 0, 40, 30});

        const bool childReportsVisible = child->isVisible();
        QVERIFY2(!childReportsVisible,
                 qPrintable(QStringLiteral(
                     "child under an already-hidden parent reports visible=%1 — "
                     "the ancestor walk would be load-bearing")
                                .arg(childReportsVisible ? 1 : 0)));

        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);

        hiddenParent->setVisible(true);           // parent shown again
        QVERIFY(child->isVisible());
        QCOMPARE(m_model->buttonAt({980, 15}), close);
    }

    void test_itemDestroyed()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *item = new QQuickItem(m_root);
        close->registerItem(item);
        close->setRect({960, 0, 40, 30});
        QCOMPARE(m_model->buttonAt({980, 15}), close);

        delete item;                              // Loader deactivates -> destroy
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);   // no stale ghost rect
    }

    void test_flagsResetWhenLastItemDestroyed()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *a = new QQuickItem(m_root);
        auto *b = new QQuickItem(m_root);
        close->registerItem(a);
        close->registerItem(b);
        close->setRect({960, 0, 40, 30});

        close->externalPress();                   // WM_NCLBUTTONDOWN happened
        close->setShowHovered(true);

        delete a;                                 // one of two gone: keep state
        QVERIFY(close->externalPressed());
        QCOMPARE(close->rect(), QRect(960, 0, 40, 30));     // rect kept

        delete b;                                 // last one gone: state invalid,
        QVERIFY(!close->externalPressed());       // a later NC release must NOT click
        QVERIFY(!close->showHovered());
        QVERIFY(close->rect().isEmpty());         // and no stale hitbox remains
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);
    }

    void test_multipleRegisteredItems()
    {
        // two topbars alive at once (MenuTopbar + LocalTopbar share the model)
        auto *min = buttonOfType(*m_model, CSDButton::Minimize);
        auto *hidden = new QQuickItem(m_root);    // invisible topbar's set
        hidden->setVisible(false);
        auto *visible = new QQuickItem(m_root);   // visible topbar's set
        min->registerItem(hidden);
        min->registerItem(visible);
        min->setRect({100, 0, 40, 30});

        QCOMPARE(m_model->buttonAt({120, 15}), min);  // visible one carries the hit

        visible->setVisible(false);               // both hidden now
        QCOMPARE(m_model->buttonAt({120, 15}), nullptr);
    }

    void test_noRegisteredItems()
    {
        // e.g. fullscreen: every Loader deactivated, nothing registered —
        // a leftover rect must not produce a hit without registered items
        buttonOfType(*m_model, CSDButton::Close)->setRect({960, 0, 40, 30});
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);
    }

    void test_registerItemIdempotent()
    {
        auto *close = buttonOfType(*m_model, CSDButton::Close);
        auto *item = new QQuickItem(m_root);
        close->registerItem(item);
        close->registerItem(item);                 // duplicate registration: no-op
        close->registerItem(nullptr);              // null: safe no-op
        close->setRect({960, 0, 40, 30});

        QCOMPARE(m_model->buttonAt({980, 15}), close);

        delete item;                               // one destroyed-connection, state not corrupted
        QCOMPARE(m_model->buttonAt({980, 15}), nullptr);
    }

private:
    std::unique_ptr<CSDButtonModel> m_model;
    QQuickItem *m_root = nullptr;
};

int main(int argc, char **argv)
{
    // CI has no desktop (cf. qml_test.cpp); an externally provided platform
    // (e.g. "windows" when running the built test on Windows) is respected
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);              // QQuickItem needs a GUI app
    TestQtCSDButtonModel tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_qt_csd_button_model.moc"
