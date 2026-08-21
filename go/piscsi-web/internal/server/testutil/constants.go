package testutil

import "net/http"

// Test constants
const (
	// SCSI configuration
	DefaultSCSIID = 6
	TestSCSIID0   = 0
	TestSCSIID1   = 1

	// File sizes
	FileSizeDefault = 1 * 1024 * 1024  // 1 MiB
	FileSizeSmall   = 512 * 1024       // 512 KiB
	FileSizeLarge   = 10 * 1024 * 1024 // 10 MiB

	// Response status
	StatusSuccess = "success"
	StatusError   = "error"

	// HTTP status codes
	StatusOK         = http.StatusOK
	StatusCreated    = http.StatusCreated
	StatusSeeOther   = http.StatusSeeOther
	StatusBadRequest = http.StatusBadRequest
)

// Endpoint paths
const (
	// Device operations
	EndpointAttach    = "/scsi/attach"
	EndpointDetach    = "/scsi/detach"
	EndpointDetachAll = "/scsi/detach_all"
	EndpointEject     = "/scsi/eject"
	EndpointInfo      = "/scsi/info"
	EndpointReserve   = "/scsi/reserve"
	EndpointRelease   = "/scsi/release"

	// File operations
	EndpointFilesCreate      = "/files/create"
	EndpointFilesDelete      = "/files/delete"
	EndpointFilesRename      = "/files/rename"
	EndpointFilesCopy        = "/files/copy"
	EndpointFilesUpload      = "/files/upload"
	EndpointFilesUploadCheck = "/files/upload/check"
	EndpointFilesDownload    = "/files/download"
	EndpointFilesDownloadURL = "/files/download_url"
	EndpointFilesExtract     = "/files/extract"
	EndpointFilesCreateISO   = "/files/create_iso"

	// Configuration
	EndpointConfigSave   = "/config/save"
	EndpointConfigLoad   = "/config/load"
	EndpointConfigDelete = "/config/delete"
	EndpointConfigAction = "/config/action"

	// Settings
	EndpointLanguage  = "/language"
	EndpointTheme     = "/theme"
	EndpointLogsLevel = "/logs/level"

	// System
	EndpointIndex = "/"
)

// Device types
const (
	DeviceTypeHDD       = "SCHD" // Hard disk
	DeviceTypeCDROM     = "SCCD" // CD-ROM
	DeviceTypeRemovable = "SCRM" // Removable media
	DeviceTypeMO        = "SCMO" // Magneto-optical
	DeviceTypeTape      = "SCTP" // Tape
	DeviceTypePrinter   = "SCLP" // Printer
	DeviceTypeHost      = "SCHS" // Host services
	DeviceTypeDaynaPort = "SCDP" // DaynaPort
)

// Flash message categories
const (
	FlashSuccess = "success"
	FlashError   = "error"
	FlashWarning = "warning"
	FlashInfo    = "info"
)

// Test configuration
const (
	TestHostname = "test-piscsi"
	TestVersion  = "test-v1.0.0"
)
