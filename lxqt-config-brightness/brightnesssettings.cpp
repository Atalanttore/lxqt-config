/*
    Copyright (C) 2016  P.L. Lucas <selairi@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "brightnesssettings.h"
#include "outputwidget.h"
#include <QMessageBox>
#include <QPushButton>

BrightnessSettings::BrightnessSettings(QWidget *parent):QDialog(parent)
{
    ui = new Ui::BrightnessSettings();
    ui->setupUi(this);

    mBrightness = new XRandrBrightness();
    mMonitors = mBrightness->getMonitorsInfo();
    mBacklight = new LXQt::Backlight(this);

    ui->backlightSlider->setEnabled(mBacklight->isBacklightAvailable() || mBacklight->isBacklightOff());
    if(mBacklight->isBacklightAvailable()) {
        ui->backlightSlider->setMaximum(mBacklight->getMaxBacklight());
        // set the minimum to 5% of the maximum to prevent a black screen
        ui->backlightSlider->setMinimum(qMax(qRound((qreal)(mBacklight->getMaxBacklight())*0.05), 1));
        ui->backlightSlider->setValue(mLastBacklightValue = mBacklight->getBacklight());
        // Don't change the backlight too quickly
        connect(ui->backlightSlider, &QSlider::valueChanged, [this] {mBacklightTimer.start();});
    }

    for(const MonitorInfo &monitor: qAsConst(mMonitors))
    {
        OutputWidget *output = new OutputWidget(monitor, this);
        ui->layout->addWidget(output);
        output->show();
        connect(output, SIGNAL(changed(MonitorInfo)), this, SLOT(monitorSettingsChanged(MonitorInfo)));
        connect(this, &BrightnessSettings::monitorReverted, output, &OutputWidget::setRevertedValues);
    }

    mBacklightTimer.setSingleShot(true);
    mBacklightTimer.setInterval(250);
    connect(&mBacklightTimer, &QTimer::timeout, this, &BrightnessSettings::setBacklight);

    mConfirmRequestTimer.setSingleShot(true);
    mConfirmRequestTimer.setInterval(1000);
    connect(&mConfirmRequestTimer, &QTimer::timeout, this, &BrightnessSettings::requestConfirmation);

}

void BrightnessSettings::setBacklight()
{
    mBacklight->setBacklight(ui->backlightSlider->value());
}

void BrightnessSettings::monitorSettingsChanged(MonitorInfo monitor)
{
    mBrightness->setMonitorsSettings(QList<MonitorInfo>{}  << monitor);
    if (ui->confirmCB->isChecked())
    {
        mConfirmRequestTimer.start();
    } else
    {
        for (auto & m : mMonitors)
        {
            if (m.id() == monitor.id() && m.name() == monitor.name())
            {
                m.setBacklight(monitor.backlight());
                m.setBrightness(monitor.brightness());
            }
        }
    }
}

void BrightnessSettings::requestConfirmation()
{
    QMessageBox msg{QMessageBox::Question, tr("Brightness settings changed")
        , tr("Confirmation required. Are the settings correct?")
        , QMessageBox::Yes | QMessageBox::No};
    int timeout = 5; // seconds
    QString no_text = msg.button(QMessageBox::No)->text();
    no_text += QStringLiteral("(%1)");
    msg.setButtonText(QMessageBox::No, no_text.arg(timeout));
    msg.setDefaultButton(QMessageBox::No);

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(false);
    timeoutTimer.setInterval(1000);
    connect(&timeoutTimer, &QTimer::timeout, [&] {
        msg.setButtonText(QMessageBox::No, no_text.arg(--timeout));
        if (timeout == 0)
        {
            timeoutTimer.stop();
            msg.reject();
        }
    });
    timeoutTimer.start();

    if (QMessageBox::Yes == msg.exec())
    {
        // re-read current values
        if(mBacklight->isBacklightAvailable())
            mLastBacklightValue = mBacklight->getBacklight();

        mMonitors = mBrightness->getMonitorsInfo();
    } else
    {
        // revert the changes
        if(mBacklight->isBacklightAvailable()) {
            disconnect(ui->backlightSlider, &QSlider::valueChanged, this, &BrightnessSettings::setBacklight);
            mBacklight->setBacklight(mLastBacklightValue);
            ui->backlightSlider->setValue(mLastBacklightValue);
            connect(ui->backlightSlider, &QSlider::valueChanged, this, &BrightnessSettings::setBacklight);
        }

        mBrightness->setMonitorsSettings(mMonitors);
        for (const auto & monitor : qAsConst(mMonitors))
            emit monitorReverted(monitor);
    }
}
