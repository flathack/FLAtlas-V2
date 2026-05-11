#include <QtTest/QtTest>

#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "rendering/view2d/MapScene.h"
#include "rendering/view2d/SystemMapView.h"

#include <algorithm>
#include <memory>

using namespace flatlas::domain;
using namespace flatlas::rendering;

class TestSystemMapView : public QObject
{
    Q_OBJECT

private slots:
    void zoomToFitUsesSystemContentForLi01Scale();
};

namespace {

std::shared_ptr<SolarObject> makeObject(const QString &nickname, float x, float z)
{
    auto object = std::make_shared<SolarObject>();
    object->setNickname(nickname);
    object->setType(SolarObject::Station);
    object->setPosition(QVector3D(x, 0.0f, z));
    return object;
}

double fitScaleForPaddedNavMap(const SystemMapView &view, const QRectF &sceneRect)
{
    const QRect viewportRect = view.viewport()->rect().adjusted(2, 2, -2, -2);
    const double paddedWidth = sceneRect.width() * (1000.0 / 880.0);
    const double paddedHeight = sceneRect.height() * (1000.0 / 880.0);
    return std::min(static_cast<double>(viewportRect.width()) / paddedWidth,
                    static_cast<double>(viewportRect.height()) / paddedHeight);
}

}

void TestSystemMapView::zoomToFitUsesSystemContentForLi01Scale()
{
    SystemDocument document;
    document.setNavMapScale(1.5);
    document.addObject(makeObject(QStringLiteral("Li01_01"), -33270.0f, -33039.0f));
    document.addObject(makeObject(QStringLiteral("Li01_02"), 52238.0f, -78213.0f));
    document.addObject(makeObject(QStringLiteral("Li01_04"), 68666.0f, 60396.0f));
    document.addObject(makeObject(QStringLiteral("Li01_10"), -92447.0f, -12925.0f));
    document.addObject(makeObject(QStringLiteral("Li01_to_Li02"), -83176.0f, 44831.0f));

    MapScene scene;
    scene.loadDocument(&document);

    SystemMapView view;
    view.resize(1000, 800);
    view.setMapScene(&scene);
    view.show();
    QTest::qWait(20);

    view.zoomToFit();

    const double fullNavMapScale = fitScaleForPaddedNavMap(view, scene.sceneRect());
    const double fittedScale = view.transform().m11();
    QVERIFY2(fittedScale > fullNavMapScale * 1.15,
             qPrintable(QStringLiteral("Expected content fit scale > %1, got %2")
                            .arg(fullNavMapScale * 1.15)
                            .arg(fittedScale)));
}

QTEST_MAIN(TestSystemMapView)
#include "test_SystemMapView.moc"
