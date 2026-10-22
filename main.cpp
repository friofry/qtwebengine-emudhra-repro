#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QtWebEngineQuick>
#include <cstring>

// Minimal reproducer & workaround for QTBUG-58059 (Oct 2026 eMudhra G3 compromise):
//
// Usage:
//   ./qtbug58059_repro              # Default Qt 6.11 behavior -> VULNERABLE (loads emSign ECC Root CA - G3 chain without error)
//   ./qtbug58059_repro --fix        # Applies Chrome Root Store constraint override -> BLOCKED (ERR_CERT_AUTHORITY_INVALID)
//
// Target URL defaults to eMudhra's official G3 test endpoint:
//   https://testovg3.emsign.com/ (chains to emSign ECC Root CA - G3)
// Override with EMUDHRA_TEST_URL=https://... if testing against a local server.

static constexpr char kDistrustEmSignEccRootG3Flag[] =
    "--test-crs-constraints=86a1ecba089c4a8d3bbe2734c612ba341d813e043cf9e8a862cd5c57a36bbe6b:maxversionexclusive=0";

int main(int argc, char *argv[])
{
    bool applyFix = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fix") == 0)
            applyFix = true;
    }

    if (applyFix) {
        QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        if (!flags.contains(kDistrustEmSignEccRootG3Flag)) {
            if (!flags.isEmpty() && !flags.endsWith(' '))
                flags.append(' ');
            flags.append(kDistrustEmSignEccRootG3Flag);
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
        }
    }

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);

    const QUrl targetUrl = QUrl::fromUserInput(
        qEnvironmentVariable("EMUDHRA_TEST_URL", QStringLiteral("https://testovg3.emsign.com/")));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("initialTargetUrl"), targetUrl);

    const QByteArray qml = R"QML(
        import QtQuick
        import QtQuick.Window
        import QtWebEngine

        Window {
            width: 800
            height: 600
            visible: true
            property bool certErrorSeen: false

            WebEngineView {
                anchors.fill: parent
                url: initialTargetUrl

                onCertificateError: function(error) {
                    certErrorSeen = true
                    console.log("RESULT=BLOCKED_BY_CERT_ERROR"
                                + " type=" + error.type
                                + " description=\"" + error.description + "\""
                                + " url=" + error.url)
                    error.rejectCertificate()
                }

                onLoadingChanged: function(req) {
                    if (req.status === WebEngineView.LoadSucceededStatus) {
                        console.log("RESULT=VULNERABLE_LOAD_SUCCEEDED"
                                    + " certErrorSeen=" + certErrorSeen
                                    + " url=" + req.url)
                        Qt.quit()
                    } else if (req.status === WebEngineView.LoadFailedStatus) {
                        console.log("RESULT=LOAD_FAILED"
                                    + " certErrorSeen=" + certErrorSeen
                                    + " errorCode=" + req.errorCode
                                    + " errorString=" + req.errorString
                                    + " url=" + req.url)
                        Qt.quit()
                    }
                }
            }

            Timer {
                interval: 25000
                running: true
                onTriggered: {
                    console.log("RESULT=TIMEOUT")
                    Qt.quit()
                }
            }
        }
    )QML";

    engine.loadData(qml);
    return app.exec();
}
