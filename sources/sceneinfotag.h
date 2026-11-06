#pragma once

#include <QString>
#include <QColor>
#include <QMap>
#include <QJsonObject>
#include <QVariant>

enum SceneInfoTagType {
  InfoTagType_Bool,
  InfoTagType_Enum,
  InfoTagType_String,
  InfoTagType_StringList
};

struct SceneInfoTagDisplayProperties {
  QString labelTxt = "";
  QColor bgColor   = QColor(Qt::white);
  QColor txtColor  = QColor(Qt::black);
  QJsonObject toJson() const;
  static SceneInfoTagDisplayProperties fromJson(const QJsonObject& json);
};

class SceneInfoTag {
protected:
  QString m_idName;
  bool m_canManuallySet;

public:
  SceneInfoTag(QString idName, bool canManuallySet)
      : m_idName(idName), m_canManuallySet(canManuallySet) {}
  virtual ~SceneInfoTag() = default;

  QString idName() const { return m_idName; }
  bool canManuallySet() const { return m_canManuallySet; }
  void setIdName(const QString& idName) { m_idName = idName; }
  void setCanManuallySet(bool canManuallySet) {
    m_canManuallySet = canManuallySet;
  }

  virtual SceneInfoTagDisplayProperties displayProperties(
      QVariant value = QVariant()) const = 0;
  virtual SceneInfoTagType type() const  = 0;
  virtual QJsonObject toJson() const     = 0;

  virtual SceneInfoTag* clone() const = 0;
};

class BoolSceneInfoTag : public SceneInfoTag {
  SceneInfoTagDisplayProperties m_displayProperties;

public:
  BoolSceneInfoTag(QString idName, bool canManuallySet,
                   SceneInfoTagDisplayProperties displayProperties)
      : SceneInfoTag(idName, canManuallySet)
      , m_displayProperties(displayProperties) {}
  virtual SceneInfoTagDisplayProperties displayProperties(
      QVariant value = QVariant()) const final override {
    return m_displayProperties;
  }
  void setDisplayProperties(const SceneInfoTagDisplayProperties& prop) {
    m_displayProperties = prop;
  }
  virtual SceneInfoTagType type() const { return InfoTagType_Bool; }
  QJsonObject toJson() const final override;
  static BoolSceneInfoTag* fromJson(const QJsonObject& json);

  virtual SceneInfoTag* clone() const final override;
};

class EnumSceneInfoTag : public SceneInfoTag {
  QMap<QString, SceneInfoTagDisplayProperties> m_displayPropertiesMap;

public:
  EnumSceneInfoTag(QString idName, bool canManuallySet)
      : SceneInfoTag(idName, canManuallySet) {}
  void addItem(const QString value, const SceneInfoTagDisplayProperties prop) {
    m_displayPropertiesMap.insert(value, prop);
  }
  void removeItem(const QString value) { m_displayPropertiesMap.remove(value); }
  QStringList valueList() const { return m_displayPropertiesMap.keys(); }
  virtual SceneInfoTagDisplayProperties displayProperties(
      QVariant value = QVariant()) const final override {
    return m_displayPropertiesMap.value(value.toString());
  }
  virtual SceneInfoTagType type() const { return InfoTagType_Enum; }
  QJsonObject toJson() const final override;
  static EnumSceneInfoTag* fromJson(const QJsonObject& json);

  virtual SceneInfoTag* clone() const final override;
};

class StringSceneInfoTag : public SceneInfoTag {
  SceneInfoTagDisplayProperties m_displayProperties;

public:
  StringSceneInfoTag(QString idName, bool canManuallySet,
                     SceneInfoTagDisplayProperties displayProperties)
      : SceneInfoTag(idName, canManuallySet)
      , m_displayProperties(displayProperties) {}
  virtual SceneInfoTagDisplayProperties displayProperties(
      QVariant value = QVariant()) const final override {
    if (value.toString().isEmpty()) return m_displayProperties;
    return {QString("%1:%2")
                .arg(m_displayProperties.labelTxt)
                .arg(value.toString()),
            m_displayProperties.bgColor, m_displayProperties.txtColor};
  }
  void setDisplayProperties(const SceneInfoTagDisplayProperties& prop) {
    m_displayProperties = prop;
  }
  virtual SceneInfoTagType type() const { return InfoTagType_String; }
  QJsonObject toJson() const final override;
  static StringSceneInfoTag* fromJson(const QJsonObject& json);

  virtual SceneInfoTag* clone() const final override;
};

class StringListSceneInfoTag : public SceneInfoTag {
  SceneInfoTagDisplayProperties m_displayProperties;

public:
  StringListSceneInfoTag(QString idName, bool canManuallySet,
                         SceneInfoTagDisplayProperties displayProperties)
      : SceneInfoTag(idName, canManuallySet)
      , m_displayProperties(displayProperties) {}
  virtual SceneInfoTagDisplayProperties displayProperties(
      QVariant value = QVariant()) const final override {
    if (value.toStringList().isEmpty()) return m_displayProperties;
    return {QString("%1:%2")
                .arg(m_displayProperties.labelTxt)
                .arg(value.toStringList().join(", ")),
            m_displayProperties.bgColor, m_displayProperties.txtColor};
  }
  void setDisplayProperties(const SceneInfoTagDisplayProperties& prop) {
    m_displayProperties = prop;
  }
  virtual SceneInfoTagType type() const { return InfoTagType_StringList; }
  QJsonObject toJson() const final override;
  static StringListSceneInfoTag* fromJson(const QJsonObject& json);

  virtual SceneInfoTag* clone() const final override;
};
