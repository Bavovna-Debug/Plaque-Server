//
//  Plaque'n'Play
//
//  Copyright (c) 2015 Meine Werke. All rights reserved.
//

#ifndef _API_
#define _API_

#define DialogueSignature						0xC2D6D5D1D6E4D900

#define PaquetSignature                         0xC8C1D360F9F0F0F0

#define TokenBinarySize							16
#define NotificationsTokenBinarySize			32
#define NotificationsTokenStringSize			64

#define DeviceBooleanFalse						0x00
#define DeviceBooleanTrue						0x01

#define DialogueTypeAnticipant					0x00000000
#define DialogueTypeRegular						0x00000001

#define DeviceTypeAppleiPhone					0xAE1E
#define DeviceTypeAppleiPad						0xAE1D

#define DialogueVerdictWelcome					0xBED10000
#define DialogueVerdictInvalidDevice			0xBED10001
#define DialogueVerdictInvalidProfile			0xBED10002
#define DialogueVerdictNewSession				0xBED10004

#define PaquetBroadcast 		               	0x00010000
#define PaquetDisplacementOnRadar               0x00010101
#define PaquetDisplacementInSight               0x00010102
#define PaquetDisplacementOnMap                 0x00010104
#define PaquetDownloadPlaquesOnRadar            0x00010201
#define PaquetDownloadPlaquesInSight            0x00010202
#define PaquetDownloadPlaquesOnMap              0x00010204
#define PaquetPostNewPlaque                     0x00020001
#define PaquetPlaqueModifiedLocation            0x00020002
#define PaquetPlaqueModifiedOrientation         0x00020003
#define PaquetPlaqueModifiedSize                0x00020004
#define PaquetPlaqueModifiedColors              0x00020005
#define PaquetPlaqueModifiedFont	            0x00020006
#define PaquetPlaqueModifiedInscription         0x00020007
#define PaquetDownloadProfiles                  0x00030001
#define PaquetNotificationsToken                0x00040000
#define PaquetValidateProfileName               0x00040001
#define PaquetCreateProfile                     0x00040002

#define PaquetRejectBusy						0xFBFB0000
#define PaquetRejectError						0xFEFE0000

#define PaquetPlaqueStrobe						0xA0B0C0D0

#define AnticipantDeviceNameLength              40
#define AnticipantDeviceModelLength             20
#define AnticipantSystemNamelLength             20
#define AnticipantSystemVersionlLength          20

#define MinimumProfileNameLength                4
#define BonjourProfileNameLength                20
#define BonjourUserNameLength                   50
#define BonjourMD5Length                        32
#define BonjourEmailAddressLength               200

#define PaquetNotificationsTokenAccepted        0xFEFE0000
#define PaquetNotificationsTokenDeclined        0xFEFE0001

#define PaquetProfileNameAvailable              0xFEFE0000
#define PaquetProfileNameAlreadyInUse           0xFEFE0001

#define BonjourCreateSucceeded					0xFEFE0002
#define BonjourCreateProfileNameAlreadyInUse	0xFEFE0003
#define BonjourCreateProfileNameConstraint		0xFEFE0004
#define BonjourCreateProfileEmailAlreadyInUse	0xFEFE0005
#define BonjourCreateProfileEmailConstraint		0xFEFE0006

#define BroadcastDestinationOnRadar				0xABBA0001
#define BroadcastDestinationInSight				0xABBA0002
#define BroadcastDestinationOnMap				0xABBA0004

#define PaquetDisplacementSucceeded             0xFEFE0008
#define PaquetDisplacementFailed	            0xFEFE0009

#define PaquetCreatePlaqueSucceeded             0xFEFE000A
#define PaquetCreatePlaqueError                 0xFEFE000B

#endif
