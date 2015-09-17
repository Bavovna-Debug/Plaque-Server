//
//  Plaque'n'Play
//
//  Copyright (c) 2015 Meine Werke. All rights reserved.
//

#ifndef _API_
#define _API_

#define CommandAnticipant                       0xABBA2015
#define CommandRadar                            0x00000001
#define CommandDownloadPlaques                  0x00000002
#define CommandCreatePlaque                     0x00000003
#define CommandPlaqueModifiedLocation           0x00000004
#define CommandPlaqueModifiedDirection          0x00000005
#define CommandPlaqueModifiedSize               0x00000006
#define CommandPlaqueModifiedColors             0x00000007
#define CommandPlaqueModifiedInscription        0x00000020
#define BonjourValidateProfileName              0x00000030
#define BonjourCreateProfile                    0x00000040

#define BonjourAnticipantOK                     0xFEFE2015

#define BonjourMD5Length                        32
#define BonjourDeviceNameLength                 40
#define BonjourDeviceModelLength                20
#define BonjourSystemNamelLength                20
#define BonjourSystemVersionlLength             20
#define BonjourProfileNameLength                20
#define BonjourUserNameLength                   50
#define BonjourEmailAddressLength               200

#define BonjourProfileNameAvailable             0xFEFE0000
#define BonjourProfileNameAlreadyInUse          0xFEFE0001

#define BonjourCreateSucceeded					0xFEFE0002
#define BonjourCreateProfileNameAlreadyInUse	0xFEFE0003
#define BonjourCreateProfileNameConstraint		0xFEFE0004
#define BonjourCreateProfileEmailAlreadyInUse	0xFEFE0005
#define BonjourCreateProfileEmailConstraint		0xFEFE0006

#define BonjourCreatePlaqueSucceeded            0xFEFE0007
#define BonjourCreatePlaqueError	            0xFEFE0008

#endif
