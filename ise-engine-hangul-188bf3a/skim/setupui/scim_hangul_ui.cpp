#include <kdialog.h>
#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file './scim_hangul_ui.ui'
**
** Created: 일 12월 30 15:50:11 2007
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.7   edited Aug 31 2005 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "scim_hangul_ui.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <klineedit.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include "skimkeygrabber.h"

/*
 *  Constructs a ScimHangulSetting as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
ScimHangulSetting::ScimHangulSetting( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "ScimHangulSetting" );
    ScimHangulSettingLayout = new QVBoxLayout( this, 11, 6, "ScimHangulSettingLayout"); 

    groupBox1 = new QGroupBox( this, "groupBox1" );
    groupBox1->setColumnLayout(0, Qt::Vertical );
    groupBox1->layout()->setSpacing( 6 );
    groupBox1->layout()->setMargin( 11 );
    groupBox1Layout = new QVBoxLayout( groupBox1->layout() );
    groupBox1Layout->setAlignment( Qt::AlignTop );

    kcfg__IMEngine_Hangul_ShowCandidateComment = new QCheckBox( groupBox1, "kcfg__IMEngine_Hangul_ShowCandidateComment" );
    groupBox1Layout->addWidget( kcfg__IMEngine_Hangul_ShowCandidateComment );

    kcfg__IMEngine_Hangul_UseAsciiMode = new QCheckBox( groupBox1, "kcfg__IMEngine_Hangul_UseAsciiMode" );
    groupBox1Layout->addWidget( kcfg__IMEngine_Hangul_UseAsciiMode );

    kcfg__IMEngine_Hangul_CommitByWord = new QCheckBox( groupBox1, "kcfg__IMEngine_Hangul_CommitByWord" );
    groupBox1Layout->addWidget( kcfg__IMEngine_Hangul_CommitByWord );
    ScimHangulSettingLayout->addWidget( groupBox1 );

    groupBox2 = new QGroupBox( this, "groupBox2" );
    groupBox2->setColumnLayout(0, Qt::Vertical );
    groupBox2->layout()->setSpacing( 6 );
    groupBox2->layout()->setMargin( 11 );
    groupBox2Layout = new QGridLayout( groupBox2->layout() );
    groupBox2Layout->setAlignment( Qt::AlignTop );

    kcfg_IMEngine_Hangul_HanjaModeKey = new KLineEdit( groupBox2, "kcfg_IMEngine_Hangul_HanjaModeKey" );

    groupBox2Layout->addWidget( kcfg_IMEngine_Hangul_HanjaModeKey, 3, 1 );

    skimEditShortcutButton3 = new SkimEditShortcutButton( groupBox2, "skimEditShortcutButton3" );

    groupBox2Layout->addWidget( skimEditShortcutButton3, 3, 2 );

    skimEditShortcutButton2 = new SkimEditShortcutButton( groupBox2, "skimEditShortcutButton2" );

    groupBox2Layout->addWidget( skimEditShortcutButton2, 2, 2 );

    kcfg__IMEngine_Hangul_HangulHanjaKey = new KLineEdit( groupBox2, "kcfg__IMEngine_Hangul_HangulHanjaKey" );

    groupBox2Layout->addWidget( kcfg__IMEngine_Hangul_HangulHanjaKey, 2, 1 );

    textLabel2 = new QLabel( groupBox2, "textLabel2" );

    groupBox2Layout->addWidget( textLabel2, 2, 0 );

    textLabel2_2 = new QLabel( groupBox2, "textLabel2_2" );

    groupBox2Layout->addWidget( textLabel2_2, 1, 0 );

    kcfg__IMEngine_Hangul_HangulKey = new KLineEdit( groupBox2, "kcfg__IMEngine_Hangul_HangulKey" );

    groupBox2Layout->addWidget( kcfg__IMEngine_Hangul_HangulKey, 1, 1 );

    textLabel4 = new QLabel( groupBox2, "textLabel4" );

    groupBox2Layout->addWidget( textLabel4, 0, 0 );

    kcfg__IMEngine_Hangul_KeyboardLayout = new QComboBox( FALSE, groupBox2, "kcfg__IMEngine_Hangul_KeyboardLayout" );

    groupBox2Layout->addWidget( kcfg__IMEngine_Hangul_KeyboardLayout, 0, 1 );

    skimEditShortcutButton2_2 = new SkimEditShortcutButton( groupBox2, "skimEditShortcutButton2_2" );

    groupBox2Layout->addWidget( skimEditShortcutButton2_2, 1, 2 );

    textLabel5 = new QLabel( groupBox2, "textLabel5" );

    groupBox2Layout->addWidget( textLabel5, 0, 2 );

    textLabel3 = new QLabel( groupBox2, "textLabel3" );

    groupBox2Layout->addWidget( textLabel3, 3, 0 );
    ScimHangulSettingLayout->addWidget( groupBox2 );
    spacer1 = new QSpacerItem( 20, 111, QSizePolicy::Minimum, QSizePolicy::Expanding );
    ScimHangulSettingLayout->addItem( spacer1 );
    languageChange();
    resize( QSize(428, 409).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( skimEditShortcutButton2, SIGNAL( setEditorText(const QString&) ), kcfg__IMEngine_Hangul_HangulHanjaKey, SLOT( setText(const QString&) ) );
    connect( kcfg__IMEngine_Hangul_HangulHanjaKey, SIGNAL( textChanged(const QString&) ), skimEditShortcutButton2, SLOT( setShortcuts(const QString&) ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
ScimHangulSetting::~ScimHangulSetting()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void ScimHangulSetting::languageChange()
{
    setCaption( tr2i18n( "ScimHangulSetting" ) );
    groupBox1->setTitle( tr2i18n( "Options" ) );
    kcfg__IMEngine_Hangul_ShowCandidateComment->setText( tr2i18n( "Show c&andidate comment" ) );
    kcfg__IMEngine_Hangul_ShowCandidateComment->setAccel( QKeySequence( tr2i18n( "Alt+A" ) ) );
    kcfg__IMEngine_Hangul_UseAsciiMode->setText( tr2i18n( "&Use ascii input mode" ) );
    kcfg__IMEngine_Hangul_UseAsciiMode->setAccel( QKeySequence( tr2i18n( "Alt+U" ) ) );
    kcfg__IMEngine_Hangul_CommitByWord->setText( tr2i18n( "&Commit by word" ) );
    kcfg__IMEngine_Hangul_CommitByWord->setAccel( QKeySequence( tr2i18n( "Alt+C" ) ) );
    groupBox2->setTitle( tr2i18n( "Keyboard" ) );
    textLabel2->setText( tr2i18n( "Hangul to Hanja keys:" ) );
    textLabel2_2->setText( tr2i18n( "Hangul keys:" ) );
    textLabel4->setText( tr2i18n( "Keyboard layout:" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->clear();
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "2bul" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "3bul 2bu-shifted" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "3bul Final" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "3bul 390" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "3bul No-shift" ) );
    kcfg__IMEngine_Hangul_KeyboardLayout->insertItem( tr2i18n( "3bul Yetgeul" ) );
    textLabel5->setText( QString::null );
    textLabel3->setText( tr2i18n( "Hanja mode keys:" ) );
}

#include "scim_hangul_ui.moc"
