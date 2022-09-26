//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "localizer.h"
#include <cassert>
#include <string>
#include <algorithm>
#include <regex>

using namespace std;

Localizer::Localizer()
{
	// Positional string arguments are %1, %2, %3
	Add(LocalizationKey::ERROR_AUTHENTICATION, "en", "Authentication failed");
	Add(LocalizationKey::ERROR_AUTHENTICATION, "de", "Authentifizierung fehlgeschlagen");
	Add(LocalizationKey::ERROR_AUTHENTICATION, "sv", "Autentisering misslyckades");
	Add(LocalizationKey::ERROR_AUTHENTICATION, "fr", "Authentification éronnée");
	Add(LocalizationKey::ERROR_AUTHENTICATION, "es", "Fallo de autentificación");
	Add(LocalizationKey::ERROR_OPERATION, "en", "Unknown operation");
	Add(LocalizationKey::ERROR_OPERATION, "de", "Unbekannte Operation");
	Add(LocalizationKey::ERROR_OPERATION, "sv", "Okänd operation");
	Add(LocalizationKey::ERROR_OPERATION, "fr", "Opération inconnue");
	Add(LocalizationKey::ERROR_OPERATION, "es", "Operación desconocida");
	Add(LocalizationKey::ERROR_LOG_LEVEL, "en", "Invalid log level %1");
	Add(LocalizationKey::ERROR_LOG_LEVEL, "de", "Ungültiger Log-Level %1");
	Add(LocalizationKey::ERROR_LOG_LEVEL, "sv", "Ogiltig loggnivå %1");
	Add(LocalizationKey::ERROR_LOG_LEVEL, "fr", "Niveau de journalisation invalide %1");
	Add(LocalizationKey::ERROR_LOG_LEVEL, "es", "Nivel de registro %1 no válido");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_ID, "en", "Missing device ID");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_ID, "de", "Fehlende Geräte-ID");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_ID, "sv", "Enhetens ID saknas");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_ID, "fr", "ID de périphérique manquante");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_ID, "es", "Falta el ID del dispositivo");
	Add(LocalizationKey::ERROR_MISSING_FILENAME, "en", "Missing filename");
	Add(LocalizationKey::ERROR_MISSING_FILENAME, "de", "Fehlender Dateiname");
	Add(LocalizationKey::ERROR_MISSING_FILENAME, "sv", "Filnamn saknas");
	Add(LocalizationKey::ERROR_MISSING_FILENAME, "fr", "Nom de fichier manquant");
	Add(LocalizationKey::ERROR_MISSING_FILENAME, "es", "Falta el nombre del archivo");
	Add(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME, "en", "Device type %1 requires a filename");
	Add(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME, "de", "Gerätetyp %1 erfordert einen Dateinamen");
	Add(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME, "es", "El tipo de dispositivo %1 requiere un nombre de archivo");
	Add(LocalizationKey::ERROR_IMAGE_IN_USE, "en", "Image file '%1' is already being used by ID %2, unit %3");
	Add(LocalizationKey::ERROR_IMAGE_IN_USE, "de", "Image-Datei '%1' wird bereits von ID %2, Einheit %3 benutzt");
	Add(LocalizationKey::ERROR_IMAGE_IN_USE, "sv", "Skivbildfilen '%1' används redan av ID %2, LUN %3");
	Add(LocalizationKey::ERROR_IMAGE_IN_USE, "fr", "Le fichier d'image '%1' est déjà utilisé par l'ID %2, unité %3");
	Add(LocalizationKey::ERROR_IMAGE_IN_USE, "es", "El archivo de imagen '%1' ya está siendo utilizado por el ID %2, unidad %3");
	Add(LocalizationKey::ERROR_IMAGE_FILE_INFO, "en", "Can't create image file info for '%1'");
	Add(LocalizationKey::ERROR_IMAGE_FILE_INFO, "de", "Image-Datei-Information für '%1' kann nicht erzeugt werden");
	Add(LocalizationKey::ERROR_IMAGE_FILE_INFO, "es", "No se puede crear información de archivo de imagen para '%1'");
	Add(LocalizationKey::ERROR_RESERVED_ID, "en", "Device ID %1 is reserved");
	Add(LocalizationKey::ERROR_RESERVED_ID, "de", "Geräte-ID %1 ist reserviert");
	Add(LocalizationKey::ERROR_RESERVED_ID, "sv", "Enhets-ID %1 är reserverat");
	Add(LocalizationKey::ERROR_RESERVED_ID, "fr", "ID de périphérique %1 réservée");
	Add(LocalizationKey::ERROR_RESERVED_ID, "es", "El ID de dispositivo %1 está reservado");
	Add(LocalizationKey::ERROR_NON_EXISTING_DEVICE, "en", "Command for non-existing ID %1");
	Add(LocalizationKey::ERROR_NON_EXISTING_DEVICE, "de", "Kommando für nicht existente ID %1");
	Add(LocalizationKey::ERROR_NON_EXISTING_DEVICE, "sv", "Kommando för avsaknat ID %1");
	Add(LocalizationKey::ERROR_NON_EXISTING_DEVICE, "fr", "Commande pour ID %1 non-existant");
	Add(LocalizationKey::ERROR_NON_EXISTING_DEVICE, "es", "Comando para ID %1 no existente");
	Add(LocalizationKey::ERROR_NON_EXISTING_UNIT, "en", "Command for non-existing ID %1, unit %2");
	Add(LocalizationKey::ERROR_NON_EXISTING_UNIT, "de", "Kommando für nicht existente ID %1, Einheit %2");
	Add(LocalizationKey::ERROR_NON_EXISTING_UNIT, "sv", "Kommando för avsaknat ID %1, LUN %2");
	Add(LocalizationKey::ERROR_NON_EXISTING_UNIT, "fr", "Command pour ID %1, unité %2 non-existant");
	Add(LocalizationKey::ERROR_NON_EXISTING_UNIT, "es", "Comando para ID %1 inexistente, unidad %2");
	Add(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, "en", "Unknown device type %1");
	Add(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, "de", "Unbekannter Gerätetyp %1");
	Add(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, "sv", "Obekant enhetstyp: %1");
	Add(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, "fr", "Type de périphérique inconnu %1");
	Add(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, "es", "Tipo de dispositivo desconocido %1");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, "en", "Device type required for unknown extension of file '%1'");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, "de", "Gerätetyp erforderlich für unbekannte Extension der Datei '%1'");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, "sv", "Man måste ange enhetstyp för obekant filändelse '%1'");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, "fr", "Type de périphérique requis pour extension inconnue du fichier '%1'");
	Add(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, "es", "Tipo de dispositivo requerido para la extensión desconocida del archivo '%1'");
	Add(LocalizationKey::ERROR_DUPLICATE_ID, "en", "Duplicate ID %1, unit %2");
	Add(LocalizationKey::ERROR_DUPLICATE_ID, "de", "Doppelte ID %1, Einheit %2");
	Add(LocalizationKey::ERROR_DUPLICATE_ID, "sv", "Duplikat ID %1, LUN %2");
	Add(LocalizationKey::ERROR_DUPLICATE_ID, "fr", "ID %1, unité %2 dupliquée");
	Add(LocalizationKey::ERROR_DUPLICATE_ID, "es", "ID duplicado %1, unidad %2");
	Add(LocalizationKey::ERROR_DETACH, "en", "Couldn't detach device");
	Add(LocalizationKey::ERROR_DETACH, "de", "Geräte konnte nicht entfernt werden");
	Add(LocalizationKey::ERROR_DETACH, "es", "No se ha podido desconectar el dispositivo");
	Add(LocalizationKey::ERROR_EJECT_REQUIRED, "en", "Existing medium must first be ejected");
	Add(LocalizationKey::ERROR_EJECT_REQUIRED, "de", "Das vorhandene Medium muss erst ausgeworfen werden");
	Add(LocalizationKey::ERROR_EJECT_REQUIRED, "sv", "Nuvarande skiva måste utmatas först");
	Add(LocalizationKey::ERROR_EJECT_REQUIRED, "fr", "Media déjà existant doit d'abord être éjecté");
	Add(LocalizationKey::ERROR_EJECT_REQUIRED, "es", "El medio existente debe ser expulsado primero");
	Add(LocalizationKey::ERROR_DEVICE_NAME_UPDATE, "en", "Once set the device name cannot be changed anymore");
	Add(LocalizationKey::ERROR_DEVICE_NAME_UPDATE, "de", "Ein bereits gesetzter Gerätename kann nicht mehr geändert werden");
	Add(LocalizationKey::ERROR_DEVICE_NAME_UPDATE, "sv", "Enhetsnamn kan ej ändras efter att ha fastställts en gång");
	Add(LocalizationKey::ERROR_DEVICE_NAME_UPDATE, "fr", "Une fois défini, le nom de périphérique ne peut plus être changé");
	Add(LocalizationKey::ERROR_DEVICE_NAME_UPDATE, "es", "Una vez establecido el nombre del dispositivo ya no se puede cambiar");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING, "en", "Missing shutdown mode");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING, "de", "Fehlender Shutdown-Modus");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING, "sv", "Avstängningsläge saknas");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING, "fr", "Mode d'extinction manquant");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING, "es", "Falta el modo de apagado");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, "en", "Invalid shutdown mode '%1'");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, "de", "Ungültiger Shutdown-Modus '%1'");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, "sv", "Ogiltigt avstängsningsläge: '%1'");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, "fr", "Mode d'extinction invalide '%1'");
	Add(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, "es", "Modo de apagado inválido '%1'");
	Add(LocalizationKey::ERROR_SHUTDOWN_PERMISSION, "en", "Missing root permission for shutdown or reboot");
	Add(LocalizationKey::ERROR_SHUTDOWN_PERMISSION, "de", "Fehlende Root-Berechtigung für Shutdown oder Neustart");
	Add(LocalizationKey::ERROR_SHUTDOWN_PERMISSION, "sv", "Root-rättigheter saknas för att kunna stänga av eller starta om systemet");
	Add(LocalizationKey::ERROR_SHUTDOWN_PERMISSION, "fr", "Permissions root manquantes pour extinction ou redémarrage");
	Add(LocalizationKey::ERROR_SHUTDOWN_PERMISSION, "es", "Falta el permiso de root para el apagado o el reinicio");
	Add(LocalizationKey::ERROR_FILE_OPEN, "en", "Invalid or non-existing file '%1': %2");
	Add(LocalizationKey::ERROR_FILE_OPEN, "de", "Ungültige oder fehlende Datei '%1': %2");
	Add(LocalizationKey::ERROR_FILE_OPEN, "sv", "Ogiltig eller saknad fil '%1': %2");
	Add(LocalizationKey::ERROR_FILE_OPEN, "fr", "Fichier invalide ou non-existant '%1': %2");
	Add(LocalizationKey::ERROR_FILE_OPEN, "es", "Archivo inválido o inexistente '%1': %2");
	Add(LocalizationKey::ERROR_BLOCK_SIZE, "en", "Invalid block size %1 bytes");
	Add(LocalizationKey::ERROR_BLOCK_SIZE, "de", "Ungültige Blockgröße %1 Bytes");
	Add(LocalizationKey::ERROR_BLOCK_SIZE, "sv", "Ogiltig blockstorlek: %1 byte");
	Add(LocalizationKey::ERROR_BLOCK_SIZE, "fr", "Taille de bloc invalide %1 octets");
	Add(LocalizationKey::ERROR_BLOCK_SIZE, "es", "Tamaño de bloque inválido %1 bytes");
	Add(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "en", "Block size for device type %1 is not configurable");
	Add(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "de", "Blockgröße für Gerätetyp %1 ist nicht konfigurierbar");
	Add(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "sv", "Enhetstypen %1 kan inte använda andra blockstorlekar");
	Add(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "fr", "Taille de block pour le type de périphérique %1 non configurable");
	Add(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, "es", "El tamaño del bloque para el tipo de dispositivo %1 no es configurable");
	Add(LocalizationKey::ERROR_SCSI_CONTROLLER, "en", "Couldn't create SCSI controller");
	Add(LocalizationKey::ERROR_SCSI_CONTROLLER, "de", "SCSI-Controller konnte nicht erzeugt werden");
	Add(LocalizationKey::ERROR_SCSI_CONTROLLER, "es", "No se ha podido crear el controlador SCSI");
	Add(LocalizationKey::ERROR_INVALID_ID, "en", "Invalid device ID %1 (0-%2)");
	Add(LocalizationKey::ERROR_INVALID_ID, "de", "Ungültige Geräte-ID %1 (0-%2)");
	Add(LocalizationKey::ERROR_INVALID_ID, "es", "ID de dispositivo inválido %1 (0-%2)");
	Add(LocalizationKey::ERROR_INVALID_LUN, "en", "Invalid LUN %1 (0-%2)");
	Add(LocalizationKey::ERROR_INVALID_LUN, "de", "Ungültige LUN %1 (0-%2)");
	Add(LocalizationKey::ERROR_INVALID_LUN, "es", "LUN invalido %1 (0-%2)");
	Add(LocalizationKey::ERROR_LUN0, "en", "LUN 0 cannot be detached as long as there is still another LUN");
	Add(LocalizationKey::ERROR_LUN0, "de", "LUN 0 kann nicht entfernt werden, solange noch eine andere LUN existiert");
	Add(LocalizationKey::ERROR_LUN0, "es", "El LUN 0 no se puede desconectar mientras haya otro LUN");
	Add(LocalizationKey::ERROR_INITIALIZATION, "en", "Initialization of %1 device, ID %2, LUN %3 failed");
	Add(LocalizationKey::ERROR_INITIALIZATION, "de", "Initialisierung von %1-Gerät, ID %2, LUN %3 fehlgeschlagen");
	Add(LocalizationKey::ERROR_INITIALIZATION, "es", "La inicialización del dispositivo %1, ID %2, LUN %3 falló");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, "en", "%1 operation denied, %2 isn't stoppable'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, "de", "%1-Operation verweigert, %2 ist nicht stopbar'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, "es", "%1 operación denegada, %2 no se puede parar'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, "en", "%1 operation denied, %2 isn't removable'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, "de", "%1-Operation verweigert, %2 ist nicht wechselbar'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, "es", "%1 operación denegada, %2 no es removible'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, "en", "%1 operation denied, %2 isn't protectable'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, "de", "%1-Operation verweigert, %2 ist nicht schützbar'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, "es", "%1 operación denegada, %2 no es protegible'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_READY, "en", "%1 operation denied, %2 isn't ready'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_READY, "de", "%1-Operation verweigert, %2 ist nicht bereit'");
	Add(LocalizationKey::ERROR_OPERATION_DENIED_READY, "es", "%1 operación denegada, %2 no está listo'");
}

void Localizer::Add(LocalizationKey key, const string& locale, string_view value)
{
	// Safeguards against empty messages, duplicate entries and unsupported locales
	assert(locale.size());
	assert(value.size());
	assert(supported_languages.find(locale) != supported_languages.end());
	assert(localized_messages[locale][key].empty());

	localized_messages[locale][key] = value;
}

string Localizer::Localize(LocalizationKey key, const string& locale, const string& arg1, const string& arg2,
		const string &arg3) const
{
	string locale_lower = locale;
	transform(locale_lower.begin(), locale_lower.end(), locale_lower.begin(), ::tolower);

	auto it = localized_messages.find(locale_lower);
	if (it == localized_messages.end()) {
		// Try to fall back to country-indepedent locale (e.g. "en" instead of "en_US")
		if (locale_lower.length() > 2) {
			it = localized_messages.find(locale_lower.substr(0, 2));
		}
		if (it == localized_messages.end()) {
			it = localized_messages.find("en");
		}
	}

	if (it != localized_messages.end()) {
		return "Missing default localization for enum value " + to_string((int)key);
	}

	auto messages = it->second;
	string message = messages[key];

	message = regex_replace(message, regex("%1"), arg1);
	message = regex_replace(message, regex("%2"), arg2);
	message = regex_replace(message, regex("%3"), arg3);

	return message;
}
