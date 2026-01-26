/*****************************************************************************
 * customspeed.cpp : Custom Speed dialog
 ****************************************************************************
 * Copyright (C) 2025 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_input.h>

#include "dialogs/customspeed.hpp"
#include "input_manager.hpp"

#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QTime>
#include <QDateTime>

CustomSpeedDialog::CustomSpeedDialog( intf_thread_t *_p_intf )
    : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowFlags( Qt::Tool );
    setWindowTitle( qtr( "Custom Playback Speed" ) );
    setWindowRole( "vlc-custom-speed" );
    setMinimumWidth( 350 );

    QVBoxLayout *mainLayout = new QVBoxLayout( this );

    /* Header: Remaining time and current speed */
    QHBoxLayout *headerLayout = new QHBoxLayout();
    remainingTimeLabel = new QLabel( qtr( "Remaining: --:--" ) );
    currentSpeedLabel = new QLabel( qtr( "Current Speed: 1.00x" ) );
    headerLayout->addWidget( remainingTimeLabel );
    headerLayout->addStretch();
    headerLayout->addWidget( currentSpeedLabel );
    mainLayout->addLayout( headerLayout );

    /* Tab Widget */
    tabWidget = new QTabWidget( this );

    /* Tab 1: Finish In */
    QWidget *finishInTab = new QWidget();
    QGridLayout *finishInLayout = new QGridLayout( finishInTab );

    QLabel *finishInLabel = new QLabel( qtr( "Finish remaining video in:" ) );
    minutesSpinBox = new QSpinBox();
    minutesSpinBox->setRange( 1, 600 );
    minutesSpinBox->setValue( 30 );
    minutesSpinBox->setSuffix( qtr( " minutes" ) );
    minutesSpinBox->setAlignment( Qt::AlignRight );

    calculatedSpeedLabel = new QLabel( qtr( "Calculated Speed: --" ) );
    calculatedSpeedLabel->setAlignment( Qt::AlignCenter );
    QFont boldFont = calculatedSpeedLabel->font();
    boldFont.setBold( true );
    boldFont.setPointSize( boldFont.pointSize() + 2 );
    calculatedSpeedLabel->setFont( boldFont );

    finishInLayout->addWidget( finishInLabel, 0, 0 );
    finishInLayout->addWidget( minutesSpinBox, 0, 1 );
    finishInLayout->addWidget( calculatedSpeedLabel, 1, 0, 1, 2, Qt::AlignCenter );
    finishInLayout->setRowStretch( 2, 1 );

    tabWidget->addTab( finishInTab, qtr( "Finish In" ) );

    /* Tab 2: Finish By */
    QWidget *finishByTab = new QWidget();
    QVBoxLayout *finishByLayout = new QVBoxLayout( finishByTab );

    QLabel *finishByLabel = new QLabel( qtr( "Select a time to finish by:" ) );
    finishByLayout->addWidget( finishByLabel );

    timeTable = new QTableWidget();
    timeTable->setColumnCount( 2 );
    timeTable->setHorizontalHeaderLabels( QStringList() << qtr( "Time" ) << qtr( "Speed" ) );
    timeTable->setSelectionBehavior( QAbstractItemView::SelectRows );
    timeTable->setSelectionMode( QAbstractItemView::SingleSelection );
    timeTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
    timeTable->horizontalHeader()->setStretchLastSection( true );
    timeTable->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    timeTable->verticalHeader()->setVisible( false );
    timeTable->setShowGrid( false );
    timeTable->setAlternatingRowColors( true );
    finishByLayout->addWidget( timeTable );

    tabWidget->addTab( finishByTab, qtr( "Finish By" ) );

    mainLayout->addWidget( tabWidget );

    /* Buttons */
    QPushButton *applyButton = new QPushButton( qtr( "&Apply" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );
    QDialogButtonBox *buttonBox = new QDialogButtonBox;

    applyButton->setDefault( true );
    buttonBox->addButton( applyButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    mainLayout->addWidget( buttonBox );

    /* Connections */
    BUTTONACT( applyButton, close() );
    BUTTONACT( cancelButton, cancel() );
    CONNECT( minutesSpinBox, valueChanged( int ), this, onMinutesChanged( int ) );
    CONNECT( timeTable, itemSelectionChanged(), this, onTableRowSelected() );
    CONNECT( timeTable, cellDoubleClicked( int, int ), this, onTableRowDoubleClicked( int, int ) );

    /* Update timer */
    updateTimer = new QTimer( this );
    updateTimer->setInterval( 1000 );
    CONNECT( updateTimer, timeout(), this, updateInfo() );

    QVLCTools::restoreWidgetPosition( p_intf, "customspeeddialog", this );
}

CustomSpeedDialog::~CustomSpeedDialog()
{
    QVLCTools::saveWidgetPosition( p_intf, "customspeeddialog", this );
}

void CustomSpeedDialog::toggleVisible()
{
    if( !isVisible() )
    {
        if( !THEMIM->getIM()->hasInput() )
            return;  /* Don't show without active input */
        updateInfo();
        onMinutesChanged( minutesSpinBox->value() );
        populateTimeTable();
        updateTimer->start();
    }
    else
    {
        updateTimer->stop();
    }
    QVLCDialog::toggleVisible();
    if( isVisible() )
        activateWindow();
}

double CustomSpeedDialog::getRemainingSeconds()
{
    if( !THEMIM->getIM()->hasInput() )
        return 0.0;

    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input )
        return 0.0;

    int64_t i_time = var_GetInteger( p_input, "time" );
    int64_t i_length = var_GetInteger( p_input, "length" );

    if( i_length <= 0 )
        return 0.0;

    double remaining = static_cast<double>( i_length - i_time ) / static_cast<double>( CLOCK_FREQ );
    return remaining > 0.0 ? remaining : 0.0;
}

double CustomSpeedDialog::calculateSpeed( double targetSeconds )
{
    double remaining = getRemainingSeconds();
    if( remaining <= 0.0 || targetSeconds <= 0.0 )
        return 0.0;

    double speed = remaining / targetSeconds;

    /* Clamp to valid range */
    if( speed < MIN_SPEED || speed > MAX_SPEED )
        return 0.0;

    return speed;
}

void CustomSpeedDialog::updateHeader()
{
    double remaining = getRemainingSeconds();

    if( remaining > 0.0 )
    {
        int hours = static_cast<int>( remaining / 3600 );
        int minutes = static_cast<int>( (remaining - hours * 3600) / 60 );
        int seconds = static_cast<int>( remaining - hours * 3600 - minutes * 60 );

        QString timeStr;
        if( hours > 0 )
            timeStr = QString( "%1:%2:%3" )
                .arg( hours )
                .arg( minutes, 2, 10, QChar('0') )
                .arg( seconds, 2, 10, QChar('0') );
        else
            timeStr = QString( "%1:%2" )
                .arg( minutes )
                .arg( seconds, 2, 10, QChar('0') );

        remainingTimeLabel->setText( qtr( "Remaining: " ) + timeStr );
    }
    else
    {
        remainingTimeLabel->setText( qtr( "Remaining: --:--" ) );
    }

    input_thread_t *p_input = THEMIM->getInput();
    float rate = p_input ? var_GetFloat( p_input, "rate" )
                         : var_InheritFloat( THEPL, "rate" );
    currentSpeedLabel->setText( qtr( "Current Speed: " ) + formatSpeed( rate ) );
}

void CustomSpeedDialog::populateTimeTable()
{
    timeTable->setRowCount( 0 );

    double remaining = getRemainingSeconds();
    if( remaining <= 0.0 )
        return;

    QTime now = QTime::currentTime();

    /* Round up to next 5-minute increment */
    int minutesToAdd = 5 - ( now.minute() % 5 );
    if( minutesToAdd == 0 )
        minutesToAdd = 5;

    QTime startTime = now.addSecs( minutesToAdd * 60 );
    /* Round seconds to 0 */
    startTime = QTime( startTime.hour(), startTime.minute(), 0 );

    /* Add entries for the next 4 hours in 5-minute increments */
    for( int i = 0; i < 48; i++ )
    {
        QTime targetTime = startTime.addSecs( i * 5 * 60 );
        int secondsUntil = now.secsTo( targetTime );

        /* Handle day wrap-around */
        if( secondsUntil <= 0 )
            secondsUntil += 24 * 3600;

        double speed = remaining / secondsUntil;

        /* Only show times where speed is within valid range */
        if( speed >= MIN_SPEED && speed <= MAX_SPEED )
        {
            int row = timeTable->rowCount();
            timeTable->insertRow( row );

            QTableWidgetItem *timeItem = new QTableWidgetItem( targetTime.toString( "h:mm AP" ) );
            timeItem->setData( Qt::UserRole, speed );
            timeTable->setItem( row, 0, timeItem );

            QTableWidgetItem *speedItem = new QTableWidgetItem( formatSpeed( speed ) );
            timeTable->setItem( row, 1, speedItem );
        }
    }

    if( timeTable->rowCount() == 0 )
    {
        /* No valid times found - show message */
        timeTable->insertRow( 0 );
        QTableWidgetItem *msgItem = new QTableWidgetItem(
            qtr( "No valid times (speed must be 1x-4x)" ) );
        msgItem->setFlags( msgItem->flags() & ~Qt::ItemIsSelectable );
        timeTable->setItem( 0, 0, msgItem );
        timeTable->setSpan( 0, 0, 1, 2 );
    }
}

QString CustomSpeedDialog::formatSpeed( double speed )
{
    return QString::number( speed, 'f', 2 ) + "x";
}

void CustomSpeedDialog::onMinutesChanged( int minutes )
{
    double targetSeconds = minutes * 60.0;
    double speed = calculateSpeed( targetSeconds );

    if( speed > 0.0 )
    {
        calculatedSpeedLabel->setText( qtr( "Calculated Speed: " ) + formatSpeed( speed ) );
        calculatedSpeedLabel->setStyleSheet( "" );
    }
    else
    {
        double remaining = getRemainingSeconds();
        if( remaining <= 0.0 )
        {
            calculatedSpeedLabel->setText( qtr( "No video playing" ) );
        }
        else
        {
            double requiredSpeed = remaining / targetSeconds;
            if( requiredSpeed < MIN_SPEED )
            {
                calculatedSpeedLabel->setText( qtr( "Speed would be below 1x" ) );
            }
            else if( requiredSpeed > MAX_SPEED )
            {
                calculatedSpeedLabel->setText( qtr( "Speed would exceed 4x" ) );
            }
            else
            {
                calculatedSpeedLabel->setText( qtr( "Invalid speed" ) );
            }
        }
        calculatedSpeedLabel->setStyleSheet( "color: #888888;" );
    }
}

void CustomSpeedDialog::onTableRowSelected()
{
    /* Selection change - no immediate action needed */
}

void CustomSpeedDialog::onTableRowDoubleClicked( int row, int column )
{
    Q_UNUSED( column );

    QTableWidgetItem *item = timeTable->item( row, 0 );
    if( item && item->data( Qt::UserRole ).isValid() )
    {
        double speed = item->data( Qt::UserRole ).toDouble();
        if( speed >= MIN_SPEED && speed <= MAX_SPEED )
        {
            int rate = INPUT_RATE_DEFAULT / speed;
            THEMIM->getIM()->setRate( rate );
            toggleVisible();
        }
    }
}

void CustomSpeedDialog::updateInfo()
{
    updateHeader();
    onMinutesChanged( minutesSpinBox->value() );
    /* Don't repopulate table - it clears selection and times don't change much */
}

void CustomSpeedDialog::cancel()
{
    updateTimer->stop();
    toggleVisible();
}

void CustomSpeedDialog::close()
{
    updateTimer->stop();

    if( !THEMIM->getIM()->hasInput() )
    {
        toggleVisible();
        return;
    }

    double speed = 0.0;

    if( tabWidget->currentIndex() == 0 )
    {
        /* Finish In tab */
        double targetSeconds = minutesSpinBox->value() * 60.0;
        speed = calculateSpeed( targetSeconds );
    }
    else
    {
        /* Finish By tab */
        QList<QTableWidgetItem*> selected = timeTable->selectedItems();
        if( !selected.isEmpty() )
        {
            QTableWidgetItem *item = timeTable->item( selected.first()->row(), 0 );
            if( item && item->data( Qt::UserRole ).isValid() )
            {
                speed = item->data( Qt::UserRole ).toDouble();
            }
        }
    }

    if( speed >= MIN_SPEED && speed <= MAX_SPEED )
    {
        int rate = INPUT_RATE_DEFAULT / speed;
        THEMIM->getIM()->setRate( rate );
    }

    toggleVisible();
}
