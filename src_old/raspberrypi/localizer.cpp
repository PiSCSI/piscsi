//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "localizer.h"
#include <cassert>
#include <string>
#include <map>
#include <algorithm>
#include <regex>

using namespace std;

Localizer::Localizer()
{
	// Supported locales, always lower case
	supported_languages = { "en", "de", "sv", "fr", "es" };

	// Positional string arguments are %1, %2, %3
	Add(ERROR_AUTHENTICATION, "en", "Authentication failed");
	Add(ERROR_AUTHENTICATION, "de", "Authentifizierung fehlgeschlagen");
	Add(ERROR_AUTHENTICATION, "sv", "Autentisering misslyckades");
	Add(ERROR_AUTHENTICATION, "fr", "Authentification éronnée");
	Add(ERROR_AUTHENTICATION, "es", "Fallo de autentificación");
	Add(ERROR_OPERATION, "en", "Unknown operation");
	Add(ERROR_OPERATION, "de", "Unbekannte Operation");
	Add(ERROR_OPERATION, "sv", "Okänd operation");
	Add(ERROR_OPERATION, "fr", "Opération inconnue");
	Add(ERROR_OPERATION, "es", "Operación desconocida");
	Add(ERROR_LOG_LEVEL, "en", "Invalid log level %1");
	Add(ERROR_LOG_LEVEL, "de", "Ungültiger Log-Level %1");
	Add(ERROR_LOG_LEVEL, "sv", "Ogiltig loggnivå %1");
	Add(ERROR_LOG_LEVEL, "fr", "Niveau de journalisation invalide %1"); 
	Add(ERROR_LOG_LEVEL, "es", "Nivel de registro %1 no válido");
	Add(ERROR_MISSING_DEVICE_ID, "en", "Missing device ID");
	Add(ERROR_MISSING_DEVICE_ID, "de", "Fehlende Geräte-ID");
	Add(ERROR_MISSING_DEVICE_ID, "sv", "Enhetens ID saknas");
	Add(ERROR_MISSING_DEVICE_ID, "fr", "ID de périphérique manquante");
	Add(ERROR_MISSING_DEVICE_ID, "es", "Falta el ID del dispositivo");
	Add(ERROR_MISSING_FILENAME, "en", "Missing filename");
	Add(ERROR_MISSING_FILENAME, "de", "Fehlender Dateiname");
	Add(ERROR_MISSING_FILENAME, "sv", "Filnamn saknas");
	Add(ERROR_MISSING_FILENAME, "fr", "Nom de fichier manquant");
	Add(ERROR_MISSING_FILENAME, "es", "Falta el nombre del archivo");
	Add(ERROR_IMAGE_IN_USE, "en", "Image file '%1' is already being used by ID %2, unit %3");
	Add(ERROR_IMAGE_IN_USE, "de", "Image-Datei '%1' wird bereits von ID %2, Einheit %3 benutzt");
	Add(ERROR_IMAGE_IN_USE, "sv", "Skivbildfilen '%1' används redan av ID %2, LUN %3");
	Add(ERROR_IMAGE_IN_USE, "fr", "Le fichier d'image '%1' est déjà utilisé par l'ID %2, unité %3");
	Add(ERROR_IMAGE_IN_USE, "es", "El archivo de imagen '%1' ya está siendo utilizado por el ID %2, unidad %3");
	Add(ERROR_RESERVED_ID, "en", "Device ID %1 is reserved");
	Add(ERROR_RESERVED_ID, "de", "Geräte-ID %1 ist reserviert");
	Add(ERROR_RESERVED_ID, "sv", "Enhets-ID %1 är reserverat");
	Add(ERROR_RESERVED_ID, "fr", "ID de périphérique %1 réservée");
	Add(ERROR_RESERVED_ID, "es", "El ID de dispositivo %1 está reservado");
	Add(ERROR_NON_EXISTING_DEVICE, "en", "Command for non-existing ID %1");
	Add(ERROR_NON_EXISTING_DEVICE, "de", "Kommando für nicht existente ID %1");
	Add(ERROR_NON_EXISTING_DEVICE, "sv", "Kommando för avsaknat ID %1");
	Add(ERROR_NON_EXISTING_DEVICE, "fr", "Commande pour ID %1 non-existant");
	Add(ERROR_NON_EXISTING_DEVICE, "es", "Comando para ID %1 no existente");
	Add(ERROR_NON_EXISTING_UNIT, "en", "Command for non-existing ID %1, unit %2");
	Add(ERROR_NON_EXISTING_UNIT, "de", "Kommando für nicht existente ID %1, Einheit %2");
	Add(ERROR_NON_EXISTING_UNIT, "sv", "Kommando för avsaknat ID %1, LUN %2");
	Add(ERROR_NON_EXISTING_UNIT, "fr", "Command pour ID %1, unité %2 non-existant");
	Add(ERROR_NON_EXISTING_UNIT, "es", "Comando para ID %1 inexistente, unidad %2");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "en", "Unknown device type %1");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "de", "Unbekannter Gerätetyp %1");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "sv", "Obekant enhetstyp: %1");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "fr", "Type de périphérique inconnu %1");
	Add(ERROR_UNKNOWN_DEVICE_TYPE, "es", "Tipo de dispositivo desconocido %1");
	Add(ERROR_MISSING_DEVICE_TYPE, "en", "Device type required for unknown extension of file '%1'");
	Add(ERROR_MISSING_DEVICE_TYPE, "de", "Gerätetyp erforderlich für unbekannte Extension der Datei '%1'");
	Add(ERROR_MISSING_DEVICE_TYPE, "sv", "Man måste ange enhetstyp för obekant filändelse '%1'");
	Add(ERROR_MISSING_DEVICE_TYPE, "fr", "Type de périphérique requis pour extension inconnue du fichier '%1'");
	Add(ERROR_MISSING_DEVICE_TYPE, "es", "Tipo de dispositivo requerido para la extensión desconocida del archivo '%1'");
	Add(ERROR_DUPLICATE_ID, "en", "Duplicate ID %1, unit %2");
	Add(ERROR_DUPLICATE_ID, "de", "Doppelte ID %1, Einheit %2");
	Add(ERROR_DUPLICATE_ID, "sv", "Duplikat ID %1, LUN %2");
	Add(ERROR_DUPLICATE_ID, "fr", "ID %1, unité %2 dupliquée");
	Add(ERROR_DUPLICATE_ID, "es", "ID duplicado %1, unidad %2");
	Add(ERROR_SASI_SCSI, "en", "SASI and SCSI can't be used at the same time");
	Add(ERROR_SASI_SCSI, "de", "SASI und SCSI können nicht gleichzeitig verwendet werden");
	Add(ERROR_SASI_SCSI, "sv", "SASI och SCSI kan ej användas samtidigt");
	Add(ERROR_SASI_SCSI, "fr", "SASI et SCSI ne peuvent être utilisés en même temps");
	Add(ERROR_SASI_SCSI, "es", "SASI y SCSI no pueden utilizarse al mismo tiempo");
	Add(ERROR_EJECT_REQUIRED, "en", "Existing medium must first be ejected");
	Add(ERROR_EJECT_REQUIRED, "de", "Das vorhandene Medium muss erst ausgeworfen werden");
	Add(ERROR_EJECT_REQUIRED, "sv", "Nuvarande skiva måste utmatas först");
	Add(ERROR_EJECT_REQUIRED, "fr", "Media déjà existant doit d'abord être éjecté");
	Add(ERROR_EJECT_REQUIRED, "es", "El medio existente debe ser expulsado primero");
	Add(ERROR_DEVICE_NAME_UPDATE, "en", "Once set the device name cannot be changed anymore");
	Add(ERROR_DEVICE_NAME_UPDATE, "de", "Ein bereits gesetzter Gerätename kann nicht mehr geändert werden");
	Add(ERROR_DEVICE_NAME_UPDATE, "sv", "Enhetsnamn kan ej ändras efter att ha fastställts en gång");
	Add(ERROR_DEVICE_NAME_UPDATE, "fr", "Une fois défini, le nom de périphérique ne peut plus être changé");
	Add(ERROR_DEVICE_NAME_UPDATE, "es", "Una vez establecido el nombre del dispositivo ya no se puede cambiar");
	Add(ERROR_SHUTDOWN_MODE_MISSING, "en", "Missing shutdown mode");
	Add(ERROR_SHUTDOWN_MODE_MISSING, "de", "Fehlender Shutdown-Modus");
	Add(ERROR_SHUTDOWN_MODE_MISSING, "sv", "Avstängningsläge saknas");
	Add(ERROR_SHUTDOWN_MODE_MISSING, "fr", "Mode d'extinction manquant");
	Add(ERROR_SHUTDOWN_MODE_MISSING, "es", "Falta el modo de apagado");
	Add(ERROR_SHUTDOWN_MODE_INVALID, "en", "Invalid shutdown mode '%1'");
	Add(ERROR_SHUTDOWN_MODE_INVALID, "de", "Ungültiger Shutdown-Modus '%1'");
	Add(ERROR_SHUTDOWN_MODE_INVALID, "sv", "Ogiltigt avstängsningsläge: '%1'");
	Add(ERROR_SHUTDOWN_MODE_INVALID, "fr", "Mode d'extinction invalide '%1'");
	Add(ERROR_SHUTDOWN_MODE_INVALID, "es", "Modo de apagado inválido '%1'");
	Add(ERROR_SHUTDOWN_PERMISSION, "en", "Missing root permission for shutdown or reboot");
	Add(ERROR_SHUTDOWN_PERMISSION, "de", "Fehlende Root-Berechtigung für Shutdown oder Neustart");
	Add(ERROR_SHUTDOWN_PERMISSION, "sv", "Root-rättigheter saknas för att kunna stänga av eller starta om systemet");
	Add(ERROR_SHUTDOWN_PERMISSION, "fr", "Permissions root manquantes pour extinction ou redémarrage");
	Add(ERROR_SHUTDOWN_PERMISSION, "es", "Falta el permiso de root para el apagado o el reinicio");
	Add(ERROR_FILE_OPEN, "en", "Invalid or non-existing file '%1': %2");
	Add(ERROR_FILE_OPEN, "de", "Ungültige oder fehlende Datei '%1': %2");
	Add(ERROR_FILE_OPEN, "sv", "Ogiltig eller saknad fil '%1': %2");
	Add(ERROR_FILE_OPEN, "fr", "Fichier invalide ou non-existant '%1': %2");
	Add(ERROR_FILE_OPEN, "es", "Archivo inválido o inexistente '%1': %2");
	Add(ERROR_BLOCK_SIZE, "en", "Invalid block size %1 bytes");
	Add(ERROR_BLOCK_SIZE, "de", "Ungültige Blockgröße %1 Bytes");
	Add(ERROR_BLOCK_SIZE, "sv", "Ogiltig blockstorlek: %1 byte");
	Add(ERROR_BLOCK_SIZE, "fr", "Taille de bloc invalide %1 octets");
	Add(ERROR_BLOCK_SIZE, "es", "Tamaño de bloque inválido %1 bytes");
	Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "en", "Block size for device type %1 is not configurable");
	Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "de", "Blockgröße für Gerätetyp %1 ist nicht konfigurierbar");
	Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "sv", "Enhetstypen %1 kan inte använda andra blockstorlekar");
	Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "fr", "Taille de block pour le type de périphérique %1 non configurable");
	Add(ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "es", "El tamaño del bloque para el tipo de dispositivo %1 no es configurable");
}

void Localizer::Add(LocalizationKey key, const string& locale, const string& value)
{
	// Safeguards against empty messages, duplicate entries and unsupported locales
	assert(locale.size());
	assert(value.size());
	assert(supported_languages.find(locale) != supported_languages.end());
	assert(localized_messages[locale][key].empty());

	localized_messages[locale][key] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale, const string& arg1, const string& arg2,
		const string &arg3)
{
	string locale_lower = locale;
	transform(locale_lower.begin(), locale_lower.end(), locale_lower.begin(), ::tolower);

	map<LocalizationKey, string> messages = localized_messages[locale_lower];
	if (messages.empty()) {
		// Try to fall back to country-indepedent locale (e.g. "en" instead of "en_US")
		if (locale_lower.length() > 2) {
			messages = localized_messages[locale_lower.substr(0, 2)];
		}
		if (messages.empty()) {
			messages = localized_messages["en"];
		}
	}

	assert(!messages.empty());

	string message = messages[key];
	if (messages.empty()) {
		return "Missing localization for enum value " + to_string(key);
	}

	message = regex_replace(message, regex("%1"), arg1);
	message = regex_replace(message, regex("%2"), arg2);
	message = regex_replace(message, regex("%3"), arg3);

	return message;
}
