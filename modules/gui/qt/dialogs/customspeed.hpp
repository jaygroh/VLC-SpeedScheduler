/*****************************************************************************
 * customspeed.hpp : Custom Speed dialogs
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

#ifndef QVLC_CUSTOMSPEED_DIALOG_H_
#define QVLC_CUSTOMSPEED_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "util/singleton.hpp"

class QTabWidget;
class QSpinBox;
class QLabel;
class QTableWidget;
class QTimer;

class CustomSpeedDialog : public QVLCDialog, public Singleton<CustomSpeedDialog>
{
    Q_OBJECT
private:
    CustomSpeedDialog( intf_thread_t * );
    virtual ~CustomSpeedDialog();

    /* UI Elements */
    QLabel *remainingTimeLabel;
    QLabel *currentSpeedLabel;
    QTabWidget *tabWidget;

    /* Tab 1: Finish In */
    QSpinBox *minutesSpinBox;
    QLabel *calculatedSpeedLabel;

    /* Tab 2: Finish By */
    QTableWidget *timeTable;

    /* Timer for updating info */
    QTimer *updateTimer;

    /* Speed limits */
    static constexpr double MIN_SPEED = 1.0;
    static constexpr double MAX_SPEED = 4.0;

    /* Helper methods */
    double getRemainingSeconds();
    double calculateSpeed( double targetSeconds );
    void updateHeader();
    void populateTimeTable();
    QString formatSpeed( double speed );

private slots:
    void close() override;
    void cancel() override;
    void onMinutesChanged( int minutes );
    void onTableRowSelected();
    void onTableRowDoubleClicked( int row, int column );
    void updateInfo();

    friend class Singleton<CustomSpeedDialog>;
public:
    void toggleVisible();
};

#endif
