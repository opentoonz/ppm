#include "sceneinfotag.h"

#include <QtTest>

class TrackedStringTag : public StringSceneInfoTag {
  bool& m_destroyed;

public:
  explicit TrackedStringTag(bool& destroyed)
      : StringSceneInfoTag("test", true, {"Label", Qt::white, Qt::black})
      , m_destroyed(destroyed) {}
  ~TrackedStringTag() { m_destroyed = true; }
};

class SceneInfoTagTest : public QObject {
  Q_OBJECT
private slots:
  void deleteThroughBasePointer() {
    // WorkflowManager and project settings own tags through SceneInfoTag*.
    bool destroyed    = false;
    SceneInfoTag* tag = new TrackedStringTag(destroyed);
    QCOMPARE(tag->displayProperties("value").labelTxt, QString("Label:value"));
    delete tag;
    QVERIFY(destroyed);
  }

  void cloneAndDeleteTags() {
    QList<SceneInfoTag*> tags{new BoolSceneInfoTag("bool", true, {}),
                              new EnumSceneInfoTag("enum", true),
                              new StringSceneInfoTag("string", true, {}),
                              new StringListSceneInfoTag("list", true, {})};
    for (SceneInfoTag* tag : tags) {
      SceneInfoTag* copy = tag->clone();
      QCOMPARE(copy->idName(), tag->idName());
      QCOMPARE(copy->type(), tag->type());
      delete copy;
      delete tag;
    }
  }
};

QTEST_APPLESS_MAIN(SceneInfoTagTest)
#include "sceneinfotag_test.moc"
