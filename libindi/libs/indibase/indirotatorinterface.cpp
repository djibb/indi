/*
    Rotator Interface
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indirotatorinterface.h"
#include "defaultdevice.h"

#include "indilogger.h"

#include <cstring>

INDI::RotatorInterface::RotatorInterface()
{
}

void INDI::RotatorInterface::initProperties(INDI::DefaultDevice *defaultDevice, const char *groupName)
{
    strncpy(rotatorName, defaultDevice->getDeviceName(), MAXINDIDEVICE);

    // Rotator GOTO
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 0., 0., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, defaultDevice->getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", groupName, IP_RW, 0, IPS_IDLE );

    // Rotator Angle
    IUFillNumber(&RotatorAbsAngleN[0], "ANGLE", "Degrees", "%.2f", 0, 360., 10., 0.);
    IUFillNumberVector(&RotatorAbsAngleNP, RotatorAbsAngleN, 1, defaultDevice->getDeviceName(), "ABS_ROTATOR_ANGLE", "Angle", groupName, IP_RW, 0, IPS_IDLE );

    // Abort Rotator
    IUFillSwitch(&AbortRotatorS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortRotatorSP, AbortRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Sync
    IUFillNumber(&SyncRotatorN[0], "ROTATOR_SYNC_TICK", "Ticks", "%.f", 0, 100000., 0., 0.);
    IUFillNumberVector(&SyncRotatorNP, SyncRotatorN, 1, defaultDevice->getDeviceName(), "SYNC_ROTATOR", "Sync", groupName, IP_RW, 0, IPS_IDLE );

    // Home Rotator
    IUFillSwitch(&HomeRotatorS[0], "HOME", "Abort", ISS_OFF);
    IUFillSwitchVector(&HomeRotatorSP, HomeRotatorS, 1, defaultDevice->getDeviceName(), "ROTATOR_HOME", "HOME", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
}

bool INDI::RotatorInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, rotatorName) == 0)
    {
        ////////////////////////////////////////////
        // Sync
        ////////////////////////////////////////////
        if (strcmp(name, SyncRotatorNP.name) == 0)
        {
            bool rc = SyncRotator(static_cast<uint32_t>(values[0]));
            SyncRotatorNP.s = rc ? IPS_OK : IPS_ALERT;
            if (rc)
                SyncRotatorN[0].value = values[0];

            IDSetNumber(&SyncRotatorNP, nullptr);
            return true;
        }
        ////////////////////////////////////////////
        // Move Absolute Ticks
        ////////////////////////////////////////////
        else if (strcmp(name, RotatorAbsPosNP.name) == 0)
        {
           RotatorAbsPosNP.s = MoveAbsRotator(static_cast<int32_t>(values[0]));
           IDSetNumber(&RotatorAbsPosNP, nullptr);
           if (RotatorAbsPosNP.s == IPS_BUSY)
               DEBUGFDEVICE(rotatorName, INDI::Logger::DBG_SESSION, "Rotator moving to %.f ticks...", values[0]);
           return true;
        }
        ////////////////////////////////////////////
        // Move Absolute Angle
        ////////////////////////////////////////////
        else if (strcmp(name, RotatorAbsAngleNP.name) == 0)
        {
            RotatorAbsAngleNP.s = MoveAngleRotator(values[0]);
            RotatorAbsPosNP.s = RotatorAbsAngleNP.s;

            IDSetNumber(&RotatorAbsAngleNP, nullptr);
            IDSetNumber(&RotatorAbsPosNP, nullptr);
            if (RotatorAbsPosNP.s == IPS_BUSY)
                DEBUGFDEVICE(rotatorName, INDI::Logger::DBG_SESSION, "Rotator moving to %.2f degrees...", values[0]);
            return true;
        }
    }

    return false;
}

bool INDI::RotatorInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (dev != nullptr && strcmp(dev, rotatorName) == 0)
    {
        ////////////////////////////////////////////
        // Abort
        ////////////////////////////////////////////
        if (strcmp(name, AbortRotatorSP.name) == 0)
        {
            AbortRotatorSP.s = AbortRotator() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&AbortRotatorSP, nullptr);
            if (AbortRotatorSP.s == IPS_OK)
            {
                if (RotatorAbsPosNP.s != IPS_OK)
                {
                    RotatorAbsPosNP.s = IPS_OK;
                    RotatorAbsAngleNP.s = IPS_OK;
                    IDSetNumber(&RotatorAbsPosNP, nullptr);
                    IDSetNumber(&RotatorAbsAngleNP, nullptr);
                }
            }
            return true;
        }
        ////////////////////////////////////////////
        // Home
        ////////////////////////////////////////////
        else if (strcmp(name, HomeRotatorSP.name) == 0)
        {
            HomeRotatorSP.s = HomeRotator();
            IUResetSwitch(&HomeRotatorSP);
            if (HomeRotatorSP.s == IPS_BUSY)
                HomeRotatorS[0].s = ISS_ON;
            IDSetSwitch(&HomeRotatorSP, nullptr);
            return true;
        }
    }

    return false;
}

bool INDI::RotatorInterface::updateProperties(INDI::DefaultDevice *defaultDevice)
{
    if (defaultDevice->isConnected())
    {
        defaultDevice->defineNumber(&RotatorAbsPosNP);
        defaultDevice->defineNumber(&RotatorAbsAngleNP);

        if (CanAbort())
            defaultDevice->defineSwitch(&AbortRotatorSP);
        if (CanSync())
            defaultDevice->defineNumber(&SyncRotatorNP);
        if (CanHome())
            defaultDevice->defineSwitch(&HomeRotatorSP);
    }
    else
    {
        defaultDevice->deleteProperty(RotatorAbsPosNP.name);
        defaultDevice->deleteProperty(RotatorAbsAngleNP.name);

        if (CanAbort())
            defaultDevice->deleteProperty(AbortRotatorSP.name);
        if (CanSync())
            defaultDevice->deleteProperty(SyncRotatorNP.name);
        if (CanHome())
            defaultDevice->deleteProperty(HomeRotatorSP.name);
    }

    return true;
}

bool INDI::RotatorInterface::SyncRotator(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support syncing.");
    return false;
}

IPState INDI::RotatorInterface::HomeRotator()
{
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support homing.");
    return IPS_ALERT;
}

bool INDI::RotatorInterface::AbortRotator()
{
    DEBUGDEVICE(rotatorName, INDI::Logger::DBG_ERROR, "Rotator does not support abort.");
    return false;
}