/****************************************************************************
** Form interface generated from reading ui file './scim_hangul_ui.ui'
**
** Created: 일 12월 30 15:50:09 2007
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.7   edited Aug 31 2005 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef SCIMHANGULSETTING_H
#define SCIMHANGULSETTING_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class SkimEditShortcutButton;
class QGroupBox;
class QCheckBox;
class KLineEdit;
class QLabel;
class QComboBox;

class ScimHangulSetting : public QWidget
{
    Q_OBJECT

public:
    ScimHangulSetting( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~ScimHangulSetting();

    QGroupBox* groupBox1;
    QCheckBox* kcfg__IMEngine_Hangul_ShowCandidateComment;
    QCheckBox* kcfg__IMEngine_Hangul_UseAsciiMode;
    QCheckBox* kcfg__IMEngine_Hangul_CommitByWord;
    QGroupBox* groupBox2;
    KLineEdit* kcfg_IMEngine_Hangul_HanjaModeKey;
    SkimEditShortcutButton* skimEditShortcutButton3;
    SkimEditShortcutButton* skimEditShortcutButton2;
    KLineEdit* kcfg__IMEngine_Hangul_HangulHanjaKey;
    QLabel* textLabel2;
    QLabel* textLabel2_2;
    KLineEdit* kcfg__IMEngine_Hangul_HangulKey;
    QLabel* textLabel4;
    QComboBox* kcfg__IMEngine_Hangul_KeyboardLayout;
    SkimEditShortcutButton* skimEditShortcutButton2_2;
    QLabel* textLabel5;
    QLabel* textLabel3;

protected:
    QVBoxLayout* ScimHangulSettingLayout;
    QSpacerItem* spacer1;
    QVBoxLayout* groupBox1Layout;
    QGridLayout* groupBox2Layout;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // SCIMHANGULSETTING_H
