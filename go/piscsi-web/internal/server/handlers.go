// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"archive/zip"
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"math"
	"net/http"
	neturl "net/url"
	"os"
	"os/exec"
	pathpkg "path"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi/go/piscsi"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/driveprops"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
	pb "github.com/piscsi/piscsi/go/proto"
)

// serves the main control page
func (s *Server) handleIndex(c *gin.Context) {
	// Get base template data
	data := s.getBaseTemplateData(c)

	// Get device list
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ListDevices())

	var devices []map[string]interface{}
	showUnits := false
	reservedIDs, _ := data["ReservedScsiIDs"].([]int32)
	reserved := make(map[int]struct{}, len(reservedIDs))
	for _, id := range reservedIDs {
		reserved[int(id)] = struct{}{}
	}
	attachedImages := make(map[string]struct{})
	var occupiedIDs []int

	if err == nil && result.GetStatus() {
		seenOccupiedIDs := make(map[int]struct{})
		for _, device := range result.GetDevicesInfo().GetDevices() {
			id := int(device.GetId())
			if _, seen := seenOccupiedIDs[id]; !seen {
				seenOccupiedIDs[id] = struct{}{}
				occupiedIDs = append(occupiedIDs, id)
			}
		}

		// Build device list
		for id := 0; id <= 7; id++ {
			for unit := 0; unit <= 31; unit++ {
				device := map[string]interface{}{
					"ID":             id,
					"Unit":           unit,
					"DeviceName":     "",
					"DeviceType":     "",
					"FileDeviceType": "",
					"File":           "",
					"Vendor":         "",
					"Product":        "",
					"Revision":       "",
					"Reserved":       false,
					"Occupied":       false,
					"NoMedia":        false,
					"Removable":      false,
				}
				if _, isReserved := reserved[id]; isReserved && unit == 0 {
					device["Reserved"] = true
				}

				// Check if device is attached
				for _, dev := range result.GetDevicesInfo().GetDevices() {
					if int(dev.GetId()) == id && int(dev.GetUnit()) == unit {
						device["Occupied"] = true
						deviceType := dev.GetType().String()
						device["DeviceName"] = deviceType
						device["DeviceType"] = strings.ToLower(deviceType)
						device["FileDeviceType"] = deviceType
						if dev.GetFile() != nil {
							device["File"] = dev.GetFile().GetName()
							if image, ok := imageNameRelativeTo(s.config.BaseDir, dev.GetFile().GetName()); ok {
								attachedImages[image] = struct{}{}
							}
						}
						device["Vendor"] = dev.GetVendor()
						device["Product"] = dev.GetProduct()
						device["Revision"] = dev.GetRevision()
						if dev.GetProperties() != nil {
							device["Removable"] = dev.GetProperties().GetRemovable()
						}
						if dev.GetStatus() != nil {
							device["NoMedia"] = dev.GetStatus().GetRemoved()
						}
						if unit > 0 {
							showUnits = true
						}
						break
					}
				}

				// Only add device if it's ID 0-7 and either occupied or unit 0
				if unit == 0 || device["Occupied"].(bool) {
					devices = append(devices, device)
				}
			}
		}
	}

	// Get file list. The daemon mapping is the source of truth for detected
	// image types, while the web app scans its configured image directory.
	imageFileTypeMapping, _ := data["ImageFileTypeMapping"].(map[string]pb.PbDeviceType)
	files, filesBySubdir := s.getImageFiles(imageFileTypeMapping, attachedImages)
	delete(data, "ImageFileTypeMapping")
	delete(data, "ReservedScsiIDs")

	// Check if directories exist
	configDirExists := false
	imageDirExists := false
	if info, err := os.Stat(s.config.ConfigDir); err == nil && info.IsDir() {
		configDirExists = true
	}
	if info, err := os.Stat(s.config.BaseDir); err == nil && info.IsDir() {
		imageDirExists = true
	}

	// Get config files
	var configFiles []string
	if configDirExists {
		entries, err := os.ReadDir(s.config.ConfigDir)
		if err == nil {
			for _, entry := range entries {
				if !entry.IsDir() && strings.EqualFold(filepath.Ext(entry.Name()), configFileSuffix) {
					configFiles = append(configFiles, entry.Name())
				}
			}
		}
	}

	// Get free disk space
	freeDiskSpace, err := freeDiskSpaceMiB(s.config.BaseDir)
	if err != nil {
		freeDiskSpace = 0
	}

	// Reserved IDs cannot be selected as attachment targets. Occupied IDs stay
	// selectable for additional LUNs, but the default is the highest free ID.
	validScsiIds, recommendedScsiId := validSCSIIDs(reserved, occupiedIDs)

	// Build the attachment catalog from the daemon's advertised device
	// capabilities. Typed network interfaces advertise the valid DaynaPort
	// topology/profile combinations.
	var deviceTypesInfo *pb.PbDeviceTypesInfo
	if result, err := s.piscsiClient.SendCommand(cmdBuilder.GetDeviceTypesInfo()); err == nil && result.GetStatus() {
		deviceTypesInfo = result.GetDeviceTypesInfo()
	}
	var networkInterfaces []*pb.PbNetworkInterface
	if result, err := s.piscsiClient.SendCommand(cmdBuilder.GetNetworkInfo()); err == nil && result.GetStatus() {
		networkInterfaces = append(networkInterfaces, result.GetNetworkInterfacesInfo().GetInterfaces()...)
	}
	deviceTypes := s.buildDeviceCatalog(deviceTypesInfo, files, networkInterfaces)

	data["ConfigDir"] = s.config.ConfigDir
	data["ConfigDirExists"] = configDirExists
	data["ConfigFiles"] = configFiles
	data["Devices"] = devices
	data["ShowUnits"] = showUnits
	data["Files"] = files
	data["FilesBySubdir"] = filesBySubdir
	data["ImageDir"] = s.config.BaseDir
	data["ImageDirExists"] = imageDirExists
	data["ImageRootDir"] = s.config.BaseDir
	s.addTransferDirectoryData(data)
	data["ValidScsiIds"] = validScsiIds
	data["RecommendedScsiId"] = recommendedScsiId
	data["DeviceTypes"] = deviceTypes
	data["FreeDiskSpace"] = freeDiskSpace
	data["CreatableImageSuffixes"] = creatableImageSuffixes(imageFileTypeMapping)
	data["HardDiskDriveProfiles"] = s.hardDiskDriveProfiles()
	if drivers, err := s.hardDiskDriverImages(); err == nil {
		data["HardDiskDriverImages"] = drivers
	}

	c.HTML(http.StatusOK, "index.html", data)
}

// returns common data for all templates
func (s *Server) getBaseTemplateData(c *gin.Context) gin.H {
	hostname := s.systemHostname()
	ipAddress := s.systemIPAddress()
	throttleNotices := s.throttleNotices()

	// Get server version from backend
	version := "Unknown"
	validImageSuffixes := []string{}
	imageFileTypeMapping := map[string]pb.PbDeviceType{}
	scanDepth := int32(0)
	reservedIDs := []int32{}
	cmdBuilder := s.getCommandBuilder(c)
	var result *pb.PbResult
	var err error
	if s.piscsiClient != nil {
		result, err = s.piscsiClient.SendCommand(cmdBuilder.ServerInfo())
	}
	if err == nil && result != nil && result.GetStatus() {
		serverInfo := result.GetServerInfo()
		if serverInfo != nil {
			version = fmt.Sprintf("%d.%d.%d",
				serverInfo.GetVersionInfo().GetMajorVersion(),
				serverInfo.GetVersionInfo().GetMinorVersion(),
				serverInfo.GetVersionInfo().GetPatchVersion())
			imageFileTypeMapping = serverInfo.GetMappingInfo().GetMapping()
			validImageSuffixes = imageSuffixes(imageFileTypeMapping)
			scanDepth = serverInfo.GetImageFilesInfo().GetDepth()
			if serverInfo.GetReservedIdsInfo() != nil {
				reservedIDs = append(reservedIDs, serverInfo.GetReservedIdsInfo().GetIds()...)
			}
		}
	}

	locale := s.selectedLocale(c)
	theme := s.selectedTheme(c)
	session, err := s.getSession(c)

	// Get flash messages from session
	flashMessage := ""
	errorMessage := ""
	if err == nil {
		flashMessage, errorMessage = GetFlashesForTemplate(session)
		// Save session to clear flash messages
		session.Save(c.Request, c.Writer)
	}

	return gin.H{
		"Title":                "PiSCSI Control",
		"Hostname":             hostname,
		"IPAddress":            ipAddress,
		"RunningEnvironment":   runningEnvironment(),
		"ThrottleHelpURL":      throttleHelpURL,
		"ThrottleNotices":      throttleNotices,
		"Version":              version,
		"Theme":                theme,
		"Locale":               locale,
		"FlashMessage":         flashMessage,
		"ErrorMessage":         errorMessage,
		"ValidImageSuffixes":   validImageSuffixes,
		"ArchiveSuffixes":      archiveSuffixes,
		"ImageFileTypeMapping": imageFileTypeMapping,
		"ScanDepth":            scanDepth,
		"ReservedScsiIDs":      reservedIDs,
	}
}

// configures the unified response handler
type ResponseOptions struct {
	Message      string
	Error        bool
	RedirectURL  string
	Template     string
	TemplateData gin.H
}

// provides unified response handling for HTML form posts
func (s *Server) respond(c *gin.Context, opts ResponseOptions) {
	opts.Message = s.localizer.Translate(s.selectedLocale(c), opts.Message)
	if uploadProgressResponse, _ := c.Get("uploadProgressResponse"); uploadProgressResponse == true {
		statusCode := http.StatusOK
		if opts.Error {
			statusCode = http.StatusBadRequest
		}
		c.JSON(statusCode, gin.H{"error": opts.Error, "message": opts.Message})
		return
	}

	// Default redirect URL
	if opts.RedirectURL == "" {
		opts.RedirectURL = "/"
	}

	session, err := s.getSession(c)
	if err == nil {
		// Set flash message
		category := FlashSuccess
		if opts.Error {
			category = FlashError
		}
		if opts.Message != "" {
			SetFlash(session, opts.Message, category)
		}
		// Save session before any response is written
		if err := session.Save(c.Request, c.Writer); err != nil {
			s.logger.Error("Failed to save session", "error", err)
		}
	}

	// If template specified, render it instead of redirecting
	if opts.Template != "" {
		data := s.getBaseTemplateData(c)
		if opts.TemplateData != nil {
			for key, value := range opts.TemplateData {
				data[key] = value
			}
		}
		statusCode := http.StatusOK
		if opts.Error {
			statusCode = http.StatusBadRequest
		}
		c.HTML(statusCode, opts.Template, data)
		return
	}

	// Redirect to specified URL (default: index)
	// Use manual header setting to ensure session cookie is written first
	c.Header("Location", opts.RedirectURL)
	c.Status(http.StatusSeeOther)
	c.Writer.WriteHeaderNow()
}

// returns files from the web app's configured image directory,
// using the daemon's extension mapping to detect their device types.
func (s *Server) getImageFiles(mapping map[string]pb.PbDeviceType, attachedImages map[string]struct{}) ([]map[string]interface{}, map[string][]map[string]interface{}) {
	files := []map[string]interface{}{}
	filesBySubdir := make(map[string][]map[string]interface{})

	if _, err := os.Stat(s.config.BaseDir); os.IsNotExist(err) {
		return files, filesBySubdir
	}

	filepath.Walk(s.config.BaseDir, func(path string, info os.FileInfo, err error) error {
		if err != nil || info.IsDir() {
			return nil
		}

		relativePath, err := filepath.Rel(s.config.BaseDir, path)
		if err != nil {
			return nil
		}
		relativePath = filepath.ToSlash(relativePath)

		extension := strings.ToLower(strings.TrimPrefix(filepath.Ext(path), "."))
		deviceType := mapping[extension]
		isArchive := isArchiveSuffix(extension)
		if deviceType == pb.PbDeviceType_UNDEFINED && !isArchive {
			return nil
		}

		file := map[string]interface{}{
			"Name":        relativePath,
			"Size":        info.Size(),
			"DisplaySize": formatFileSize(info.Size()),
			"InUse":       imageIsAttached(relativePath, attachedImages),
			"IsArchive":   isArchive,
		}
		if deviceType != pb.PbDeviceType_UNDEFINED {
			file["DetectedType"] = deviceType.String()
			file["DetectedTypeName"] = deviceTypeName(deviceType)
		}
		if properties, ok := s.readImageProperties(relativePath); ok {
			file["Properties"] = properties
			file["PropertiesFile"] = relativePath + ".properties"
		}
		if isArchive {
			if members, err := s.inspectArchive(path, info); err == nil {
				file["ArchiveContents"] = members
			} else if s.logger != nil {
				file["ArchiveError"] = err.Error()
				s.logger.Warn("Could not inspect archive", "path", path, "error", err)
			} else {
				file["ArchiveError"] = err.Error()
			}
		}

		files = append(files, file)

		// Group by subdirectory
		dir := filepath.Dir(relativePath)
		if dir == "." {
			dir = s.config.BaseDir
		} else {
			dir = filepath.Join(s.config.BaseDir, filepath.FromSlash(dir))
		}
		filesBySubdir[dir] = append(filesBySubdir[dir], file)

		return nil
	})

	return files, filesBySubdir
}

// mirrors the Python client's grouping of daemon-provided extensions:
// hard disks, removable disks, magneto-optical disks, CD-ROMs, then tapes.
func imageSuffixes(mapping map[string]pb.PbDeviceType) []string {
	deviceTypes := []pb.PbDeviceType{
		pb.PbDeviceType_SCHD,
		pb.PbDeviceType_SCRM,
		pb.PbDeviceType_SCMO,
		pb.PbDeviceType_SCCD,
		pb.PbDeviceType_SCTP,
	}

	suffixes := make([]string, 0, len(mapping))
	for _, deviceType := range deviceTypes {
		group := []string{}
		for suffix, mappedType := range mapping {
			if mappedType == deviceType {
				group = append(group, suffix)
			}
		}
		sort.Strings(group)
		suffixes = append(suffixes, group...)
	}

	return suffixes
}

type creatableImageSuffix struct {
	Suffix      string
	Description string
}

// returns the daemon-advertised image formats that can be represented by an empty file.
// CD-ROM images have a separate creation workflow, while HDI and NHD need
// format-specific headers.
func creatableImageSuffixes(mapping map[string]pb.PbDeviceType) []creatableImageSuffix {
	deviceTypes := []pb.PbDeviceType{
		pb.PbDeviceType_SCHD,
		pb.PbDeviceType_SCRM,
		pb.PbDeviceType_SCMO,
		pb.PbDeviceType_SCTP,
	}
	result := make([]creatableImageSuffix, 0, len(mapping))
	for _, deviceType := range deviceTypes {
		group := make([]string, 0)
		seen := make(map[string]struct{})
		for suffix, mappedType := range mapping {
			suffix = strings.ToLower(strings.TrimPrefix(suffix, "."))
			if mappedType == deviceType && suffix != "hdi" && suffix != "nhd" {
				if _, exists := seen[suffix]; exists {
					continue
				}
				seen[suffix] = struct{}{}
				group = append(group, suffix)
			}
		}
		sort.Strings(group)
		if deviceType == pb.PbDeviceType_SCHD {
			sort.Sort(sort.Reverse(sort.StringSlice(group)))
		}
		for _, suffix := range group {
			result = append(result, creatableImageSuffix{
				Suffix:      suffix,
				Description: imageSuffixDescription(suffix, deviceType),
			})
		}
	}
	return result
}

func imageSuffixDescription(suffix string, deviceType pb.PbDeviceType) string {
	descriptions := map[string]string{
		"hds": "Hard Disk Image (Generic)",
		"hda": "Hard Disk Image (Apple)",
		"hdn": "Hard Disk Image (NEC)",
		"hd1": "Hard Disk Image (SCSI-1)",
		"hdr": "Removable Disk Image",
		"mos": "Magneto-Optical Disk Image",
		"tap": "Tape Image",
		"tar": "Tape Archive",
	}
	if description, ok := descriptions[suffix]; ok {
		return description
	}
	return deviceTypeName(deviceType) + " Image"
}

func (s *Server) hardDiskDriveProfiles() []string {
	if s.driveProps == nil {
		return nil
	}
	profiles := make([]string, 0)
	for _, drive := range s.driveProps.GetAllDrives() {
		if drive.DeviceType == pb.PbDeviceType_SCHD.String() {
			profiles = append(profiles, drive.Name)
		}
	}
	sort.Strings(profiles)
	return profiles
}

func deviceTypeName(deviceType pb.PbDeviceType) string {
	switch deviceType {
	case pb.PbDeviceType_SCHD:
		return "SCSI Hard Disk"
	case pb.PbDeviceType_SCRM:
		return "SCSI Removable Media"
	case pb.PbDeviceType_SCMO:
		return "SCSI Magneto-Optical"
	case pb.PbDeviceType_SCCD:
		return "SCSI CD-ROM"
	case pb.PbDeviceType_SCDP:
		return "Ethernet Adapter"
	case pb.PbDeviceType_SCLP:
		return "Printer"
	case pb.PbDeviceType_SCHS:
		return "Host Services"
	case pb.PbDeviceType_SCTP:
		return "SCSI Tape"
	default:
		return deviceType.String()
	}
}

type deviceParameterControl struct {
	Key     string
	Default string
	Kind    string
	Options []string
}

type deviceCatalogEntry struct {
	Key           string
	Name          string
	Removable     bool
	SupportsFile  bool
	Parameters    []deviceParameterControl
	DaynaProfiles []daynaPortProfile
	Files         []map[string]interface{}
	DriveProfiles []string
}

type daynaPortProfile struct {
	Mode      string
	Interface string
	Label     string
}

// converts DEVICE_TYPES_INFO into the complete attachment catalog
// rendered by the dashboard.
func (s *Server) buildDeviceCatalog(
	info *pb.PbDeviceTypesInfo,
	files []map[string]interface{},
	networkInterfaces []*pb.PbNetworkInterface,
) []deviceCatalogEntry {
	if info == nil {
		return nil
	}

	catalog := make([]deviceCatalogEntry, 0, len(info.GetProperties()))
	for _, typeProperties := range info.GetProperties() {
		deviceType := typeProperties.GetType()
		if deviceType == pb.PbDeviceType_UNDEFINED {
			continue
		}

		properties := typeProperties.GetProperties()
		entry := deviceCatalogEntry{
			Key:          deviceType.String(),
			Name:         deviceTypeName(deviceType),
			Removable:    properties.GetRemovable(),
			SupportsFile: properties.GetSupportsFile(),
		}

		paramKeys := make([]string, 0, len(properties.GetDefaultParams()))
		for key := range properties.GetDefaultParams() {
			paramKeys = append(paramKeys, key)
		}
		sort.Strings(paramKeys)
		for _, key := range paramKeys {
			if deviceType == pb.PbDeviceType_SCDP && (key == "interface" || key == "mode") {
				continue
			}
			defaultValue := properties.GetDefaultParams()[key]
			control := deviceParameterControl{
				Key:     key,
				Default: defaultValue,
				Kind:    "text",
			}
			if key == "interface" {
				control.Kind = "interface"
				for _, networkInterface := range networkInterfaces {
					if networkInterface.GetUp() {
						control.Options = append(control.Options, networkInterface.GetName())
					}
				}
			} else if _, err := strconv.Atoi(defaultValue); err == nil {
				control.Kind = "number"
			}
			entry.Parameters = append(entry.Parameters, control)
		}
		if deviceType == pb.PbDeviceType_SCDP {
			for _, networkInterface := range networkInterfaces {
				if !networkInterface.GetUp() {
					continue
				}
				for _, mode := range networkInterface.GetSupportedMode() {
					profile := daynaPortProfile{Mode: mode, Interface: networkInterface.GetName()}
					switch mode {
					case "bridge":
						profile.Label = "Wired bridge"
					case "proxyarp":
						profile.Label = "Wi-Fi proxy ARP"
					default:
						continue
					}
					entry.DaynaProfiles = append(entry.DaynaProfiles, profile)
				}
			}
			sort.Slice(entry.DaynaProfiles, func(i, j int) bool {
				if entry.DaynaProfiles[i].Mode == entry.DaynaProfiles[j].Mode {
					return entry.DaynaProfiles[i].Interface < entry.DaynaProfiles[j].Interface
				}
				return entry.DaynaProfiles[i].Mode == "bridge"
			})
		}

		if entry.SupportsFile {
			for _, file := range files {
				if detectedType, ok := file["DetectedType"].(string); ok && detectedType == entry.Key {
					entry.Files = append(entry.Files, file)
				}
			}
			if s.driveProps != nil {
				for _, drive := range s.driveProps.GetAllDrives() {
					if drive.DeviceType == entry.Key {
						entry.DriveProfiles = append(entry.DriveProfiles, drive.Name)
					}
				}
				sort.Strings(entry.DriveProfiles)
			}
		}

		catalog = append(catalog, entry)
	}

	return catalog
}

// returns server health status
func (s *Server) handleHealthcheck(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status": "healthy",
	})
}

// attaches a SCSI device
func (s *Server) handleAttach(c *gin.Context) {
	// Parse form data
	scsiID := c.PostForm("scsi_id")
	unit := c.DefaultPostForm("unit", "0")
	deviceType := c.PostForm("type")
	imageName := c.PostForm("file")
	if imageName == "" {
		imageName = c.PostForm("file_name")
	}
	file := imageName
	if file != "" {
		var err error
		file, err = resolveImagePath(s.config.BaseDir, file)
		if err != nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Invalid image file path",
			})
			return
		}
	}

	if scsiID == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "SCSI ID is required",
		})
		return
	}

	// Parse IDs
	id, err := parseIntParam(scsiID)
	if err != nil || id < 0 || id > 7 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid SCSI ID (must be 0-7)",
		})
		return
	}

	lun, err := parseIntParam(unit)
	if err != nil || lun < 0 || lun > 31 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid LUN (must be 0-31)",
		})
		return
	}

	// Map device type string to protobuf enum
	pbType, err := parseDeviceType(deviceType)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid device type",
		})
		return
	}

	// Build additional parameters
	params := make(map[string]string)
	if vendor := c.PostForm("vendor"); vendor != "" {
		params["vendor"] = vendor
	}
	if product := c.PostForm("product"); product != "" {
		params["product"] = product
	}
	if revision := c.PostForm("revision"); revision != "" {
		params["revision"] = revision
	}
	for formKey, values := range c.Request.PostForm {
		if !strings.HasPrefix(formKey, "param_") {
			continue
		}
		paramKey := strings.TrimPrefix(formKey, "param_")
		if paramKey == "" {
			continue
		}
		for _, value := range values {
			if value != "" {
				params[paramKey] = value
				break
			}
		}
	}

	cmdBuilder := s.getCommandBuilder(c)

	// DaynaPort attachments always use an explicit mode/interface pair. The
	// profile selector is the web form representation; direct clients may send
	// param_mode and param_interface instead.
	profileMessage := ""
	if pbType == pb.PbDeviceType_SCDP {
		if profile := c.PostForm("daynaport_profile"); profile != "" {
			mode, interfaceName, profileErr := parseDaynaPortProfile(profile)
			if profileErr != nil {
				s.respond(c, ResponseOptions{Error: true, Message: profileErr.Error()})
				return
			}
			if requestedMode, ok := params["mode"]; ok && requestedMode != mode {
				s.respond(c, ResponseOptions{Error: true, Message: "DaynaPort mode does not match the selected profile"})
				return
			}
			if requestedInterface, ok := params["interface"]; ok && requestedInterface != interfaceName {
				s.respond(c, ResponseOptions{Error: true, Message: "DaynaPort interface does not match the selected profile"})
				return
			}
			params["mode"] = mode
			params["interface"] = interfaceName
		}

		mode := params["mode"]
		interfaceName := params["interface"]
		if mode == "" || interfaceName == "" {
			s.respond(c, ResponseOptions{Error: true, Message: "Select a DaynaPort network profile (mode and interface)"})
			return
		}
		networkResult, networkErr := s.piscsiClient.SendCommand(cmdBuilder.GetNetworkInfo())
		if networkErr != nil {
			s.respond(c, ResponseOptions{Error: true, Message: "Failed to validate network interfaces: " + networkErr.Error()})
			return
		}
		var selectedInterface *pb.PbNetworkInterface
		for _, available := range networkResult.GetNetworkInterfacesInfo().GetInterfaces() {
			if available.GetName() == interfaceName {
				selectedInterface = available
				break
			}
		}
		if !networkResult.GetStatus() || selectedInterface == nil {
			s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Network interface %s is not available", interfaceName)})
			return
		}
		profileReady, message := daynaPortProfileStatus(mode, selectedInterface)
		if !profileReady {
			s.respond(c, ResponseOptions{Error: true, Message: message})
			return
		}
		profileMessage = message
	}

	identity, err := s.attachmentIdentity(imageName, c.PostForm("drive_name"), pbType)
	if err != nil {
		s.respond(c, ResponseOptions{Error: true, Message: err.Error()})
		return
	}
	if identity.vendor != "" {
		params["vendor"] = identity.vendor
	}
	if identity.product != "" {
		params["product"] = identity.product
	}
	if identity.revision != "" {
		params["revision"] = identity.revision
	}

	// Inserting media is distinct from attaching a device. Only switch to
	// INSERT when the target is the same removable type and currently has no
	// media.
	insertMedia := false
	if removableDeviceType(pbType) && file != "" {
		devicesResult, listErr := s.piscsiClient.SendCommand(cmdBuilder.ListDevices())
		if listErr != nil {
			s.respond(c, ResponseOptions{Error: true, Message: "Failed to inspect attached devices: " + listErr.Error()})
			return
		}
		if !devicesResult.GetStatus() {
			s.respond(c, ResponseOptions{Error: true, Message: devicesResult.GetMsg()})
			return
		}
		for _, attached := range devicesResult.GetDevicesInfo().GetDevices() {
			if attached.GetId() != id || attached.GetUnit() != lun ||
				attached.GetProperties() == nil || !attached.GetProperties().GetRemovable() ||
				attached.GetStatus() == nil || !attached.GetStatus().GetRemoved() {
				continue
			}
			if attached.GetType() != pbType {
				s.respond(c, ResponseOptions{
					Error: true,
					Message: fmt.Sprintf(
						"Cannot insert %s media into the existing %s device at SCSI ID %d:%d",
						pbType, attached.GetType(), id, lun,
					),
				})
				return
			}
			insertMedia = true
			break
		}
	}

	command := cmdBuilder.AttachDevice(id, lun, pbType, file, identity.blockSize, params)
	if insertMedia {
		command = cmdBuilder.InsertMedia(id, lun, pbType, file, params)
	}
	result, err := s.piscsiClient.SendCommand(command)

	if err != nil {
		s.logger.Error("Failed to attach device", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}

	// Check result status
	if !result.Status {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: result.Msg,
		})
		return
	}

	// Success message with device details
	action := "Attached"
	if insertMedia {
		action = "Inserted media into"
	}
	message := fmt.Sprintf("%s %s device at SCSI ID %s:%s", action, deviceType, scsiID, unit)
	if file != "" {
		message = fmt.Sprintf("%s %s (%s) at SCSI ID %s:%s", action, deviceType, file, scsiID, unit)
	}
	if profileMessage != "" {
		message += " - " + profileMessage
	}

	s.respond(c, ResponseOptions{
		Message: message,
	})
}

// detaches a SCSI device
func (s *Server) handleDetach(c *gin.Context) {
	// Parse form data
	scsiID := c.PostForm("scsi_id")
	unit := c.DefaultPostForm("unit", "0")

	if scsiID == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "SCSI ID is required",
		})
		return
	}

	id, err := parseIntParam(scsiID)
	if err != nil || id < 0 || id > 7 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid SCSI ID (must be 0-7)",
		})
		return
	}

	lun, err := parseIntParam(unit)
	if err != nil || lun < 0 || lun > 31 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid LUN (must be 0-31)",
		})
		return
	}

	// Send detach command
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(
		cmdBuilder.DetachDevice(id, lun),
	)

	if err != nil {
		s.logger.Error("Failed to detach device", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}

	if !result.Status {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: result.Msg,
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Detached device from SCSI ID %s:%s", scsiID, unit),
	})
}

// detaches all SCSI devices
func (s *Server) handleDetachAll(c *gin.Context) {
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.DetachAll())

	if err != nil {
		s.logger.Error("Failed to detach all devices", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}

	if !result.Status {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: result.Msg,
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: "Detached all devices",
	})
}

// ejects removable media
func (s *Server) handleEject(c *gin.Context) {
	scsiID := c.PostForm("scsi_id")
	unit := c.DefaultPostForm("unit", "0")

	if scsiID == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "SCSI ID is required",
		})
		return
	}

	id, err := parseIntParam(scsiID)
	if err != nil || id < 0 || id > 7 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid SCSI ID (must be 0-7)",
		})
		return
	}

	lun, err := parseIntParam(unit)
	if err != nil || lun < 0 || lun > 31 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid LUN (must be 0-31)",
		})
		return
	}

	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(
		cmdBuilder.EjectDevice(id, lun),
	)

	if err != nil {
		s.logger.Error("Failed to eject device", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}

	if !result.Status {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: result.Msg,
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Ejected media from SCSI ID %s:%s", scsiID, unit),
	})
}

// handles file uploads
func (s *Server) handleFilesUpload(c *gin.Context) {
	if c.GetHeader("X-Requested-With") == "XMLHttpRequest" {
		c.Set("uploadProgressResponse", true)
	}

	if s.config.MaxFileSize < 0 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid upload size configuration",
		})
		return
	}

	// Leave room for multipart headers and the small destination fields while
	// still bounding the complete request body.
	requestLimit := s.config.MaxFileSize
	if requestLimit <= math.MaxInt64-(1<<20) {
		requestLimit += 1 << 20
	} else {
		requestLimit = math.MaxInt64
	}
	c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, requestLimit)

	reader, err := c.Request.MultipartReader()
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid multipart upload",
		})
		return
	}

	formValues := map[string]string{
		"destination": "disk_images",
	}
	var (
		filename string
		fileSize int64
		tempFile *os.File
		tempPath string
		fullPath string
	)
	defer func() {
		if tempFile != nil {
			_ = tempFile.Close()
		}
		if tempPath != "" {
			_ = os.Remove(tempPath)
		}
	}()

	for {
		part, partErr := reader.NextPart()
		if partErr == io.EOF {
			break
		}
		if partErr != nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to read multipart upload",
			})
			return
		}

		if part.FileName() == "" {
			const maxFieldSize = 4096
			value, readErr := io.ReadAll(io.LimitReader(part, maxFieldSize+1))
			_ = part.Close()
			if readErr != nil || len(value) > maxFieldSize {
				s.respond(c, ResponseOptions{
					Error:   true,
					Message: "Invalid upload form field",
				})
				return
			}

			switch part.FormName() {
			case "destination", "images_subdir", "shared_subdir":
				if tempFile != nil {
					s.respond(c, ResponseOptions{
						Error:   true,
						Message: "Upload destination fields must precede the file",
					})
					return
				}
				formValues[part.FormName()] = string(value)
			}
			continue
		}

		if part.FormName() != "file" || tempFile != nil {
			_ = part.Close()
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Invalid file upload",
			})
			return
		}

		filename = filepath.Base(part.FileName())
		if !isValidFilename(filename) {
			_ = part.Close()
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Invalid filename",
			})
			return
		}

		destPath, destErr := s.uploadDestination(formValues)
		if destErr != nil {
			_ = part.Close()
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: destErr.Error(),
			})
			return
		}
		if mkdirErr := os.MkdirAll(destPath, 0755); mkdirErr != nil {
			_ = part.Close()
			s.logger.Error("Failed to create directory", "error", mkdirErr, "path", destPath)
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to create destination directory",
			})
			return
		}

		fullPath = filepath.Join(destPath, filename)
		if _, statErr := os.Lstat(fullPath); statErr == nil {
			_ = part.Close()
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "The file already exists",
			})
			return
		} else if !os.IsNotExist(statErr) {
			_ = part.Close()
			s.logger.Error("Failed to inspect upload destination", "error", statErr, "path", fullPath)
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to inspect upload destination",
			})
			return
		}

		tempFile, err = os.CreateTemp(destPath, ".piscsi-upload-*")
		if err != nil {
			_ = part.Close()
			s.logger.Error("Failed to create temporary upload file", "error", err, "path", destPath)
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to create temporary upload file",
			})
			return
		}
		tempPath = tempFile.Name()

		copyLimit := s.config.MaxFileSize
		if copyLimit < math.MaxInt64 {
			copyLimit++
		}
		fileSize, err = io.CopyBuffer(tempFile, io.LimitReader(part, copyLimit), make([]byte, 64*1024))
		_ = part.Close()
		if err != nil {
			s.logger.Error("Failed to stream uploaded file", "error", err, "path", tempPath)
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to write uploaded file",
			})
			return
		}
		if fileSize > s.config.MaxFileSize {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("File too large (max %d bytes)", s.config.MaxFileSize),
			})
			return
		}
	}

	if tempFile == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "No file uploaded",
		})
		return
	}
	if err := tempFile.Sync(); err != nil {
		s.logger.Error("Failed to sync uploaded file", "error", err, "path", tempPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to finalize uploaded file",
		})
		return
	}
	if err := tempFile.Chmod(0644); err != nil {
		s.logger.Error("Failed to set uploaded file permissions", "error", err, "path", tempPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to finalize uploaded file",
		})
		return
	}
	if err := tempFile.Close(); err != nil {
		s.logger.Error("Failed to close uploaded file", "error", err, "path", tempPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to finalize uploaded file",
		})
		return
	}
	tempFile = nil

	// A hard link publishes the completed file atomically and, unlike Rename,
	// fails if another request created the destination while this one streamed.
	if err := os.Link(tempPath, fullPath); err != nil {
		if os.IsExist(err) {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "The file already exists",
			})
			return
		}
		s.logger.Error("Failed to publish uploaded file", "error", err, "path", fullPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to finalize uploaded file",
		})
		return
	}
	if err := os.Remove(tempPath); err != nil {
		s.logger.Warn("Failed to remove temporary upload link", "error", err, "path", tempPath)
	} else {
		tempPath = ""
	}

	s.logger.Info("File uploaded", "filename", filename, "size", fileSize, "destination", filepath.Dir(fullPath))
	s.respond(c, ResponseOptions{
		Message:     "File uploaded successfully",
		RedirectURL: "/upload",
	})
}

// checks whether an upload target already exists before a client begins uploading it
func (s *Server) handleFilesUploadCheck(c *gin.Context) {
	filename := filepath.Base(c.PostForm("filename"))
	if !isValidFilename(filename) {
		c.JSON(http.StatusBadRequest, gin.H{"error": true, "message": "Invalid filename"})
		return
	}

	formValues := map[string]string{
		"destination":   c.DefaultPostForm("destination", "disk_images"),
		"images_subdir": c.PostForm("images_subdir"),
		"shared_subdir": c.PostForm("shared_subdir"),
	}
	destPath, err := s.uploadDestination(formValues)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": true, "message": err.Error()})
		return
	}

	_, err = os.Lstat(filepath.Join(destPath, filename))
	if err == nil {
		c.JSON(http.StatusOK, gin.H{"exists": true})
		return
	}
	if os.IsNotExist(err) {
		c.JSON(http.StatusOK, gin.H{"exists": false})
		return
	}

	s.logger.Error("Failed to inspect upload destination", "error", err, "path", destPath)
	c.JSON(http.StatusInternalServerError, gin.H{"error": true, "message": "Failed to inspect upload destination"})
}

func (s *Server) uploadDestination(formValues map[string]string) (string, error) {
	switch formValues["destination"] {
	case "images", "disk_images":
		destPath, err := uploadDestinationPath(s.config.BaseDir, formValues["images_subdir"])
		if err != nil {
			return "", fmt.Errorf("invalid upload subdirectory")
		}
		return destPath, nil
	case "shared", "shared_files":
		destPath, err := uploadDestinationPath(s.config.SharedDir, formValues["shared_subdir"])
		if err != nil {
			return "", fmt.Errorf("invalid upload subdirectory")
		}
		return destPath, nil
	case "config", "piscsi_config":
		return s.config.ConfigDir, nil
	default:
		return "", fmt.Errorf("invalid destination")
	}
}

// handles file downloads
func (s *Server) handleFilesDownload(c *gin.Context) {
	filename := c.Query("file")
	if filename == "" {
		filename = c.PostForm("file")
	}
	if filename == "" {
		c.String(http.StatusBadRequest, "Filename is required")
		return
	}

	// Check which directory to download from
	source := c.Query("source")
	if source == "" {
		source = c.DefaultPostForm("source", "images")
	}
	var sourcePath string
	switch source {
	case "images":
		sourcePath = s.config.BaseDir
	case "shared":
		sourcePath = s.config.SharedDir
	case "config":
		sourcePath = s.config.ConfigDir
	default:
		c.String(http.StatusBadRequest, "Invalid source")
		return
	}

	realPath, err := resolvePathWithin(sourcePath, filename)
	if err != nil {
		c.String(http.StatusBadRequest, "Invalid filename")
		return
	}

	// Check if file exists
	if _, err := os.Stat(realPath); os.IsNotExist(err) {
		c.String(http.StatusNotFound, "File not found")
		return
	}

	c.FileAttachment(realPath, filepath.Base(filename))
}

// saves current configuration
func (s *Server) handleConfigSave(c *gin.Context) {
	filename := c.PostForm("filename")
	if filename == "" {
		filename = c.PostForm("name")
	}
	filename, err := normalizeConfigFilename(filename)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: err.Error(),
		})
		return
	}

	// SERVER_INFO provides the version, attached devices, and reserved IDs
	// needed by the established JSON configuration format.
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ServerInfo())
	if err != nil {
		s.logger.Error("Failed to get server info for config save", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}
	if !result.GetStatus() || result.GetServerInfo() == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "PiSCSI did not return server information: " + result.GetMsg(),
		})
		return
	}

	configContent, err := marshalConfiguration(result.GetServerInfo())
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to encode configuration: " + err.Error(),
		})
		return
	}

	// Ensure config directory exists
	if err := os.MkdirAll(s.config.ConfigDir, 0755); err != nil {
		s.logger.Error("Failed to create config directory", "error", err)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to create configuration directory",
		})
		return
	}

	// Write configuration file
	fullPath := filepath.Join(s.config.ConfigDir, filename)
	if err := os.WriteFile(fullPath, configContent, 0644); err != nil {
		s.logger.Error("Failed to write config file", "error", err, "path", fullPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to write configuration file",
		})
		return
	}

	s.logger.Info("Configuration saved", "filename", filename)
	s.respond(c, ResponseOptions{
		Message: "Configuration saved successfully",
	})
}

// loads a configuration
func (s *Server) handleConfigLoad(c *gin.Context) {
	filename := c.PostForm("filename")
	if filename == "" {
		filename = c.PostForm("name")
	}
	filename, err := normalizeConfigFilename(filename)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: err.Error(),
		})
		return
	}

	if err := s.loadConfigurationFile(c, filename); err != nil {
		s.logger.Error("Failed to load configuration", "error", err, "filename", filename)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to load configuration: " + err.Error(),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: "Configuration loaded from " + filename,
	})
}

// creates a command builder with the configured daemon token
// and the locale selected for the current browser session.
func (s *Server) getCommandBuilder(c *gin.Context) *piscsi.CommandBuilder {
	return s.newCommandBuilder(s.selectedLocale(c))
}

// Helper functions

// parses a string parameter to int32
func parseIntParam(s string) (int32, error) {
	var val int
	_, err := fmt.Sscanf(s, "%d", &val)
	if err != nil {
		return 0, err
	}
	return int32(val), nil
}

// converts a string device type to protobuf enum
func parseDeviceType(deviceType string) (pb.PbDeviceType, error) {
	enumValue, ok := pb.PbDeviceType_value[strings.ToUpper(deviceType)]
	if !ok || pb.PbDeviceType(enumValue) == pb.PbDeviceType_UNDEFINED {
		return pb.PbDeviceType_UNDEFINED, fmt.Errorf("unknown device type: %s", deviceType)
	}
	return pb.PbDeviceType(enumValue), nil
}

// checks if a filename is safe (no path traversal, no special chars)
func isValidFilename(filename string) bool {
	// Check for empty filename
	if filename == "" {
		return false
	}

	// Check for path traversal attempts
	if strings.Contains(filename, "..") || strings.Contains(filename, "/") || strings.Contains(filename, "\\") {
		return false
	}

	// Check for hidden files (starting with .)
	if strings.HasPrefix(filename, ".") {
		return false
	}

	// Check filename length (reasonable limit)
	if len(filename) > 255 {
		return false
	}

	return true
}

// retrieves detailed info for all attached devices
func (s *Server) handleScsiInfo(c *gin.Context) {
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ListDevices())
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to get device info: %v", err),
		})
		return
	}

	devicesInfo := result.GetDevicesInfo()
	if !result.GetStatus() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: result.GetMsg(),
		})
		return
	}

	templateDevices := make([]map[string]interface{}, 0)
	for _, device := range devicesInfo.GetDevices() {
		image := ""
		imageSize := uint64(0)
		if device.GetFile() != nil {
			image = device.GetFile().GetName()
			imageSize = device.GetFile().GetSize()
		}
		status := ""
		if device.GetStatus() != nil {
			status = device.GetStatus().String()
		}
		templateDevices = append(templateDevices, map[string]interface{}{
			"ID":         device.GetId(),
			"Unit":       device.GetUnit(),
			"DeviceType": device.GetType().String(),
			"Vendor":     device.GetVendor(),
			"Product":    device.GetProduct(),
			"Revision":   device.GetRevision(),
			"BlockSize":  device.GetBlockSize(),
			"Image":      image,
			"Size":       imageSize,
			"Status":     status,
			"Params":     formatDeviceParams(device.GetParams()),
		})
	}

	message := "Retrieved device information"
	if len(templateDevices) == 0 {
		message = "No devices attached"
	}
	s.respond(c, ResponseOptions{
		Message:  message,
		Template: "deviceinfo.html",
		TemplateData: gin.H{
			"Title":   "PiSCSI Device Info",
			"Devices": templateDevices,
		},
	})
}

// formatDeviceParams returns the display form of a device parameter map.
func formatDeviceParams(params map[string]string) string {
	return strings.TrimSuffix(strings.TrimPrefix(fmt.Sprint(params), "map["), "]")
}

// reserves a SCSI ID
func (s *Server) handleScsiReserve(c *gin.Context) {
	scsiIDStr := c.PostForm("scsi_id")

	// Parse and validate SCSI ID
	scsiID, err := parseIntParam(scsiIDStr)
	if err != nil || scsiID < 0 || scsiID > 7 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid SCSI ID (must be 0-7)",
		})
		return
	}

	// Get current reserved IDs
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ServerInfo())
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to get server info: %v", err),
		})
		return
	}

	// SERVER_INFO returns reservations nested inside PbServerInfo.
	currentIDs, ok := reservedIDsFromServerInfoResult(result)
	if !ok {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to get reserved SCSI IDs: " + result.GetMsg(),
		})
		return
	}

	// Check if ID is already reserved
	for _, id := range currentIDs {
		if id == scsiID {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("SCSI ID %d is already reserved", scsiID),
			})
			return
		}
	}

	// Add new ID to reserved list
	newIDs := append(currentIDs, scsiID)

	// Send reserve command
	result, err = s.piscsiClient.SendCommand(cmdBuilder.ReserveIDs(newIDs))
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to reserve SCSI ID: %v", err),
		})
		return
	}

	// Check if command succeeded
	if !result.GetStatus() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to reserve SCSI ID: %s", result.GetMsg()),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Reserved SCSI ID %d", scsiID),
	})
}

// releases a reserved SCSI ID
func (s *Server) handleScsiRelease(c *gin.Context) {
	scsiIDStr := c.PostForm("scsi_id")

	// Parse and validate SCSI ID
	scsiID, err := parseIntParam(scsiIDStr)
	if err != nil || scsiID < 0 || scsiID > 7 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid SCSI ID (must be 0-7)",
		})
		return
	}

	// Get current reserved IDs
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ServerInfo())
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to get server info: %v", err),
		})
		return
	}

	// SERVER_INFO returns reservations nested inside PbServerInfo.
	currentIDs, ok := reservedIDsFromServerInfoResult(result)
	if !ok {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to get reserved SCSI IDs: " + result.GetMsg(),
		})
		return
	}

	// Remove the ID from reserved list
	newIDs := make([]int32, 0)
	found := false
	for _, id := range currentIDs {
		if id != scsiID {
			newIDs = append(newIDs, id)
		} else {
			found = true
		}
	}

	if !found {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("SCSI ID %d is not currently reserved", scsiID),
		})
		return
	}

	// Send reserve command with updated list
	result, err = s.piscsiClient.SendCommand(cmdBuilder.ReserveIDs(newIDs))
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to release SCSI ID: %v", err),
		})
		return
	}

	// Check if command succeeded
	if !result.GetStatus() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to release SCSI ID: %s", result.GetMsg()),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Released the reservation for SCSI ID %d", scsiID),
	})
}

// deletes a file from the images directory
func (s *Server) handleFilesDelete(c *gin.Context) {
	fileName := c.PostForm("file_name")
	if fileName == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name is required",
		})
		return
	}

	fullPath, err := resolvePathWithin(s.config.BaseDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}

	// Check if file exists
	if _, err := os.Stat(fullPath); os.IsNotExist(err) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("File not found: %s", fileName),
		})
		return
	}

	_, propPath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid properties filename",
		})
		return
	}

	propDeleted, err := deleteImageAndProperties(fullPath, propPath)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to delete image and properties consistently: %v", err),
		})
		return
	}

	message := fmt.Sprintf("Image file deleted: %s", fileName)
	if propDeleted {
		message = fmt.Sprintf("Image file with properties deleted: %s", fileName)
	}

	s.respond(c, ResponseOptions{
		Message: message,
	})
}

// renames a file in the images directory
func (s *Server) handleFilesRename(c *gin.Context) {
	fileName := c.PostForm("file_name")
	newFileName := c.PostForm("new_file_name")

	if fileName == "" || newFileName == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name and new file name are required",
		})
		return
	}

	// Validate both filenames
	oldPath, err := resolvePathWithin(s.config.BaseDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}
	newPath, err := resolvePathWithin(s.config.BaseDir, newFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid new filename",
		})
		return
	}

	// Check if source file exists
	if _, err := os.Stat(oldPath); os.IsNotExist(err) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("File not found: %s", fileName),
		})
		return
	}

	_, oldPropPath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid properties filename",
		})
		return
	}
	_, newPropPath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, newFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid new properties filename",
		})
		return
	}

	propRenamed, err := renameImageAndProperties(oldPath, newPath, oldPropPath, newPropPath)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to rename image and properties consistently: %v", err),
		})
		return
	}

	message := fmt.Sprintf("Image file renamed to: %s", newFileName)
	if propRenamed {
		message = fmt.Sprintf("Image file with properties renamed to: %s", newFileName)
	}

	s.respond(c, ResponseOptions{
		Message: message,
	})
}

// creates a copy of a file in the images directory
func (s *Server) handleFilesCopy(c *gin.Context) {
	fileName := c.PostForm("file_name")
	copyFileName := c.PostForm("copy_file_name")

	if fileName == "" || copyFileName == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name and copy file name are required",
		})
		return
	}

	// Validate both filenames
	srcPath, err := resolvePathWithin(s.config.BaseDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}
	dstPath, err := resolvePathWithin(s.config.BaseDir, copyFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid copy filename",
		})
		return
	}

	// Check if source file exists
	if _, err := os.Stat(srcPath); os.IsNotExist(err) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("File not found: %s", fileName),
		})
		return
	}

	_, srcPropPath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid properties filename",
		})
		return
	}
	_, dstPropPath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, copyFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid copy properties filename",
		})
		return
	}

	propCopied, err := copyImageAndProperties(srcPath, dstPath, srcPropPath, dstPropPath)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to copy image and properties consistently: %v", err),
		})
		return
	}

	message := fmt.Sprintf("Copy of image file saved as: %s", copyFileName)
	if propCopied {
		message = fmt.Sprintf("Copy of image file with properties saved as: %s", copyFileName)
	}

	s.respond(c, ResponseOptions{
		Message: message,
	})
}

// downloads a configuration file
func (s *Server) handleFilesDownloadConfig(c *gin.Context) {
	fileName := c.PostForm("file")
	if fileName == "" {
		c.String(http.StatusBadRequest, "File name is required")
		return
	}

	// Validate filename
	if !isValidFilename(fileName) {
		c.String(http.StatusBadRequest, "Invalid filename")
		return
	}

	// Construct full path
	fullPath := filepath.Join(s.config.ConfigDir, fileName)

	// Verify path is within config directory
	cleanPath := filepath.Clean(fullPath)
	configDir := filepath.Clean(s.config.ConfigDir)
	if !strings.HasPrefix(cleanPath, configDir) {
		c.String(http.StatusBadRequest, "Invalid file path")
		return
	}

	// Check if file exists
	if _, err := os.Stat(fullPath); os.IsNotExist(err) {
		c.String(http.StatusNotFound, "File not found: %s", fileName)
		return
	}

	// Serve the file for download
	c.Header("Content-Description", "File Transfer")
	c.Header("Content-Disposition", fmt.Sprintf("attachment; filename=%s", fileName))
	c.File(fullPath)
}

// performs an action on a configuration file (load, delete, or send)
func (s *Server) handleConfigAction(c *gin.Context) {
	fileName := c.PostForm("name")
	var err error
	fileName, err = normalizeConfigFilename(fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: err.Error(),
		})
		return
	}

	fullPath := filepath.Join(s.config.ConfigDir, fileName)

	// Check which action to perform
	if c.PostForm("load") != "" {
		if err := s.loadConfigurationFile(c, fileName); err != nil {
			s.logger.Error("Failed to load configuration", "error", err, "filename", fileName)
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: "Failed to load configuration: " + err.Error(),
			})
			return
		}

		s.respond(c, ResponseOptions{
			Message: fmt.Sprintf("Configuration loaded from %s", fileName),
		})
		return
	}

	if c.PostForm("delete") != "" {
		// Delete configuration file
		if _, err := os.Stat(fullPath); os.IsNotExist(err) {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("File not found: %s", fileName),
			})
			return
		}

		if err := os.Remove(fullPath); err != nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("Failed to delete file: %v", err),
			})
			return
		}

		s.respond(c, ResponseOptions{
			Message: fmt.Sprintf("Configuration file deleted: %s", fileName),
		})
		return
	}

	if c.PostForm("send") != "" {
		// Download configuration file
		if _, err := os.Stat(fullPath); os.IsNotExist(err) {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("File not found: %s", fileName),
			})
			return
		}

		c.Header("Content-Description", "File Transfer")
		c.Header("Content-Disposition", fmt.Sprintf("attachment; filename=%s", fileName))
		c.File(fullPath)
		return
	}

	// No recognized action
	s.respond(c, ResponseOptions{
		Error:   true,
		Message: "No known operation in request. Expected one of: load, delete, send",
	})
}

// sets the PiSCSI backend log level
func (s *Server) handleLogsLevel(c *gin.Context) {
	level := c.DefaultPostForm("level", "info")

	// Validate log level
	validLevels := map[string]bool{
		"trace":   true,
		"debug":   true,
		"info":    true,
		"warning": true,
		"error":   true,
	}

	if !validLevels[level] {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Invalid log level: %s (must be trace, debug, info, warning, or error)", level),
		})
		return
	}

	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.SetLogLevel(level))
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to set log level: %v", err),
		})
		return
	}

	if !result.GetStatus() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to set log level: %s", result.GetMsg()),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Log level set to %s", level),
	})
}

// displays system logs
func (s *Server) handleLogsShow(c *gin.Context) {
	lines := c.DefaultPostForm("lines", "100")
	scope := c.PostForm("scope")

	// Build journalctl command
	args := []string{}
	if lines != "" {
		args = append(args, "-n", lines)
	}
	if scope != "" {
		args = append(args, "-u", scope)
	}

	// Execute journalctl command
	output, err := s.runSystemCommand("journalctl", args...)

	logs := string(output)
	if err != nil {
		message := fmt.Sprintf("Failed to fetch system logs: %v", err)
		if details := strings.TrimSpace(logs); details != "" {
			message += ": " + details
		}
		data := s.getBaseTemplateData(c)
		data["Title"] = "PiSCSI System Logs"
		data["ErrorMessage"] = message
		c.HTML(http.StatusInternalServerError, "logs.html", data)
		return
	}

	// Prepare scope display text
	scopeDisplay := "All logs"
	if scope != "" {
		scopeDisplay = scope
	}

	// Render the logs template
	data := s.getBaseTemplateData(c)
	data["Scope"] = scopeDisplay
	data["Lines"] = lines
	data["Logs"] = logs
	data["Title"] = "PiSCSI System Logs"

	c.HTML(http.StatusOK, "logs.html", data)
}

// restarts the system
func (s *Server) handleSysReboot(c *gin.Context) {
	s.handleHostPowerOperation(c, "reboot", "System reboot initiated")
}

// shuts down the system
func (s *Server) handleSysShutdown(c *gin.Context) {
	s.handleHostPowerOperation(c, "system", "System shutdown initiated")
}

func (s *Server) handleHostPowerOperation(c *gin.Context, mode, successMessage string) {
	result, err := s.piscsiClient.SendCommand(s.getCommandBuilder(c).ShutDown(mode))
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Failed to communicate with piscsi daemon: " + err.Error(),
		})
		return
	}
	if !result.GetStatus() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "PiSCSI rejected the power operation: " + result.GetMsg(),
		})
		return
	}
	s.respond(c, ResponseOptions{
		Message: successMessage,
	})
}

// creates a blank disk image file
func (s *Server) handleFilesCreate(c *gin.Context) {
	fileName := strings.TrimSpace(c.PostForm("file_name"))
	sizeStr := c.PostForm("size") // Size in MB
	fileType := strings.ToLower(strings.TrimPrefix(c.DefaultPostForm("type", "hds"), "."))
	driveName := c.PostForm("drive_name")
	driveFormat := c.PostForm("drive_format")

	if fileName == "" || sizeStr == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name and size are required",
		})
		return
	}

	// Validate filename
	if !isValidFilename(fileName) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}

	// Validate the daemon-derived suffix rather than accepting arbitrary file
	// extensions from a crafted request.
	mapping := s.imageFileTypeMapping(c)
	allowedSuffix := false
	for _, imageType := range creatableImageSuffixes(mapping) {
		if imageType.Suffix == fileType {
			allowedSuffix = true
			break
		}
	}
	if !allowedSuffix {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Unsupported image file type",
		})
		return
	}

	// Validate formatting before creating anything, so a malformed request
	// cannot leave an unformatted image behind.
	if !s.isSupportedDriveFormat(driveFormat) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("%s is not a valid hard disk format", driveFormat),
		})
		return
	}
	if driveFormat != "" && mappedImageType(mapping, fileType) != pb.PbDeviceType_SCHD {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Formatting is only supported for hard disk images",
		})
		return
	}

	var selectedDrive *driveprops.DriveProperty
	if driveName != "" {
		if s.driveProps == nil {
			s.respond(c, ResponseOptions{Error: true, Message: "Drive properties database not available"})
			return
		}
		var err error
		selectedDrive, err = s.driveProps.GetByName(driveName)
		if err != nil || selectedDrive.DeviceType != pb.PbDeviceType_SCHD.String() {
			s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("No hard disk properties data for drive %s", driveName)})
			return
		}
	}

	sizeMB, err := strconv.ParseInt(sizeStr, 10, 64)
	if err != nil || sizeMB < 1 || sizeMB > 262144 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid size",
		})
		return
	}

	fullFileName := fmt.Sprintf("%s.%s", fileName, fileType)
	fullPath, propFilePath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fullFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid file path",
		})
		return
	}

	if selectedDrive != nil {
		if _, err := os.Stat(propFilePath); err == nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("Properties file already exists: %s.properties", fullFileName),
			})
			return
		}
	}

	sizeBytes := sizeMB * 1024 * 1024
	imageFile, err := os.OpenFile(fullPath, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o644)
	if err != nil {
		message := fmt.Sprintf("Failed to create image file: %v", err)
		if os.IsExist(err) {
			message = fmt.Sprintf("File already exists: %s", fullFileName)
		}
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: message,
		})
		return
	}
	if err := imageFile.Truncate(sizeBytes); err != nil {
		_ = imageFile.Close()
		_ = os.Remove(fullPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to size image file: %v", err),
		})
		return
	}
	if err := imageFile.Close(); err != nil {
		_ = os.Remove(fullPath)
		s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Failed to finish image file: %v", err)})
		return
	}

	cleanupImage := true
	defer func() {
		if cleanupImage {
			_ = os.Remove(fullPath)
		}
	}()

	if driveFormat != "" {
		if err := s.formatNewImage(fullPath, sizeMB, driveFormat); err != nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("Failed to format image file: %v", err),
			})
			return
		}
	}

	if selectedDrive != nil {
		if err := writeJSONAtomically(propFilePath, drivePropertyData(selectedDrive)); err != nil {
			s.respond(c, ResponseOptions{
				Error:   true,
				Message: fmt.Sprintf("Failed to write properties file: %v", err),
			})
			return
		}
	}
	cleanupImage = false

	formatPostfix := ""
	if driveFormat != "" {
		formatPostfix = fmt.Sprintf(" (%s)", driveFormat)
	}
	message := fmt.Sprintf("Image file created: %s%s", fullFileName, formatPostfix)
	if selectedDrive != nil {
		message = fmt.Sprintf("Image file with properties created: %s%s", fullFileName, formatPostfix)
	}
	s.respond(c, ResponseOptions{
		Message: message,
	})
}

func (s *Server) imageFileTypeMapping(c *gin.Context) map[string]pb.PbDeviceType {
	result, err := s.piscsiClient.SendCommand(s.getCommandBuilder(c).ServerInfo())
	if err != nil || !result.GetStatus() || result.GetServerInfo() == nil {
		return nil
	}
	return result.GetServerInfo().GetMappingInfo().GetMapping()
}

// hardDiskDriverImages returns the regular driver-image files immediately in
// DRIVER_DIR. Driver files are deliberately not searched recursively: the
// selected filename is later used to open a file in this directory.
func (s *Server) hardDiskDriverImages() ([]string, error) {
	entries, err := os.ReadDir(s.config.DriverDir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}

	drivers := make([]string, 0, len(entries))
	for _, entry := range entries {
		info, err := entry.Info()
		if err != nil || !info.Mode().IsRegular() {
			continue
		}
		if isHardDiskDriverImage(entry.Name()) {
			drivers = append(drivers, entry.Name())
		}
	}
	sort.Strings(drivers)
	return drivers, nil
}

func (s *Server) isSupportedDriveFormat(format string) bool {
	switch format {
	case "", "FAT16", "FAT32":
		return true
	}

	drivers, err := s.hardDiskDriverImages()
	if err != nil {
		return false
	}
	for _, driver := range drivers {
		if format == driver {
			return true
		}
	}
	return false
}

func isHardDiskDriverImage(name string) bool {
	extension := strings.ToLower(filepath.Ext(name))
	return extension == ".bin" || extension == ".img"
}

func mappedImageType(mapping map[string]pb.PbDeviceType, suffix string) pb.PbDeviceType {
	for mappedSuffix, deviceType := range mapping {
		if strings.EqualFold(strings.TrimPrefix(mappedSuffix, "."), suffix) {
			return deviceType
		}
	}
	return pb.PbDeviceType_UNDEFINED
}

func (s *Server) formatNewImage(fullPath string, sizeMB int64, format string) error {
	volumeName := fmt.Sprintf("HD %dM", sizeMB)
	switch format {
	case "FAT16", "FAT32":
		fatSize := strings.TrimPrefix(format, "FAT")
		partitionType := "6"
		if fatSize == "32" {
			partitionType = "b"
		}
		if err := runFormatterCommand("fdisk", strings.Join([]string{
			"o", "n", "p", "", "2048", "", "t", partitionType, "w", "",
		}, "\n"), fullPath); err != nil {
			return err
		}
		// Formatting at the partition offset avoids privileged loop-device and
		// device-mapper operations in the unprivileged web service.
		return runFormatterCommand(
			"mkfs.fat", "", "-v", "-F", fatSize, "-n",
			volumeName[:min(len(volumeName), 11)], "--offset=2048", fullPath,
		)
	default:
		if !isHardDiskDriverImage(format) {
			return fmt.Errorf("unsupported hard disk format %q", format)
		}
		driverPartitionType := "Apple_Driver"
		if isMiniSCSIDriver(format) {
			driverPartitionType = "Apple_Driver43"
		}
		if err := runFormatterCommand("hfdisk", strings.Join([]string{
			"i", "", "C", "", "32", "Driver_Partition", driverPartitionType,
			"C", "", "", volumeName, "Apple_HFS", "w", "y", "p", "",
		}, "\n"), fullPath); err != nil {
			return err
		}
		driverPath := filepath.Join(s.config.DriverDir, format)
		if isMiniSCSIDriver(format) {
			if err := injectMiniSCSIDriver(fullPath, driverPath); err != nil {
				return err
			}
			return runHFSFormatterCommand("-l", volumeName, fullPath, "1")
		}
		if err := injectHFSDriver(fullPath, driverPath); err != nil {
			return err
		}
		return runHFSFormatterCommand("-l", volumeName, fullPath, "1")
	}
}

func runHFSFormatterCommand(args ...string) error {
	// hfsutils stores its current-volume state in $HOME/.hcwd, even for
	// hformat. The service account has no writable home, and sharing this
	// state between concurrent formatter processes would also introduce a
	// race, so give each invocation a private temporary home.
	homeDir, err := os.MkdirTemp("", "piscsi-hfsutils-*")
	if err != nil {
		return fmt.Errorf("create hfsutils working directory: %w", err)
	}
	defer os.RemoveAll(homeDir)

	environment := make([]string, 0, len(os.Environ())+1)
	for _, variable := range os.Environ() {
		if !strings.HasPrefix(variable, "HOME=") {
			environment = append(environment, variable)
		}
	}
	environment = append(environment, "HOME="+homeDir)

	_, err = runFormatterCommandWithEnvironment("hformat", "", environment, args...)
	return err
}

func runFormatterCommand(name, stdin string, args ...string) error {
	_, err := runFormatterCommandWithInput(name, stdin, args...)
	return err
}

func runFormatterCommandWithInput(name, stdin string, args ...string) (string, error) {
	return runFormatterCommandWithEnvironment(name, stdin, nil, args...)
}

func runFormatterCommandWithEnvironment(name, stdin string, environment []string, args ...string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, name, args...)
	if environment != nil {
		cmd.Env = environment
	}
	if stdin != "" {
		cmd.Stdin = strings.NewReader(stdin)
	}
	output, err := cmd.CombinedOutput()
	if ctx.Err() == context.DeadlineExceeded {
		return string(output), fmt.Errorf("%s timed out", name)
	}
	if err != nil {
		detail := strings.TrimSpace(string(output))
		if detail != "" {
			return string(output), fmt.Errorf("%s failed: %s", name, detail)
		}
		return string(output), fmt.Errorf("%s failed: %w", name, err)
	}
	return string(output), nil
}

func injectHFSDriver(imagePath, driverPath string) error {
	driver, err := os.Open(driverPath)
	if err != nil {
		return fmt.Errorf("open hard disk driver %q: %w", driverPath, err)
	}
	defer driver.Close()
	image, err := os.OpenFile(imagePath, os.O_WRONLY, 0)
	if err != nil {
		return err
	}
	defer image.Close()
	if _, err := image.Seek(64*512, io.SeekStart); err != nil {
		return err
	}
	if _, err := io.CopyN(image, driver, 32*512); err != nil {
		return fmt.Errorf("inject hard disk driver: %w", err)
	}
	return image.Sync()
}

const (
	diskBlockSize                 = 512
	miniSCSIDriverPartitionBlock  = 64
	miniSCSIDriverPartitionBlocks = 32
	partitionMapEntrySize         = 512
	partitionMapSignature         = 0x504d
	partitionTypeOffset           = 0x30
	partitionStatusOffset         = 0x58
	partitionBootStartOffset      = 0x5c
	partitionBootSizeOffset       = 0x60
	partitionBootAddressOffset    = 0x64
	partitionBootEntryOffset      = 0x6c
	partitionBootChecksumOffset   = 0x74
	partitionProcessorOffset      = 0x78
	partitionStatusInUse          = 1 << 2
	partitionStatusHasBootInfo    = 1 << 3
	partitionStatusBootCodePIC    = 1 << 6
)

// isMiniSCSIDriver identifies MiniSCSI driver blobs by the documented filename
// convention. Unlike legacy 16 KiB driver images, MiniSCSI is a variable-size
// Apple_Driver43 boot-code blob with partition-map boot metadata.
func isMiniSCSIDriver(name string) bool {
	return strings.HasPrefix(strings.ToLower(filepath.Base(name)), "miniscsi")
}

func injectMiniSCSIDriver(imagePath, driverPath string) error {
	driver, err := os.ReadFile(driverPath)
	if err != nil {
		return fmt.Errorf("read MiniSCSI driver %q: %w", driverPath, err)
	}
	if len(driver) == 0 || len(driver) > miniSCSIDriverPartitionBlocks*diskBlockSize {
		return fmt.Errorf("MiniSCSI driver size %d is outside the supported range 1-%d bytes", len(driver), miniSCSIDriverPartitionBlocks*diskBlockSize)
	}

	image, err := os.OpenFile(imagePath, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	defer image.Close()
	if _, err := image.Seek(miniSCSIDriverPartitionBlock*diskBlockSize, io.SeekStart); err != nil {
		return err
	}
	if _, err := image.Write(driver); err != nil {
		return fmt.Errorf("inject MiniSCSI driver: %w", err)
	}
	if err := updateMiniSCSIPartitionMap(image, len(driver), miniSCSIChecksum(driver)); err != nil {
		return err
	}
	return image.Sync()
}

func updateMiniSCSIPartitionMap(image *os.File, driverSize int, checksum uint16) error {
	entry := make([]byte, partitionMapEntrySize)
	if _, err := image.ReadAt(entry, diskBlockSize); err != nil {
		return fmt.Errorf("read Apple partition map: %w", err)
	}
	if binary.BigEndian.Uint16(entry) != partitionMapSignature {
		return errors.New("invalid Apple partition map signature")
	}
	entryCount := binary.BigEndian.Uint32(entry[4:])
	if entryCount == 0 {
		return errors.New("Apple partition map has no entries")
	}

	for block := uint32(1); block <= entryCount; block++ {
		if _, err := image.ReadAt(entry, int64(block)*diskBlockSize); err != nil {
			return fmt.Errorf("read Apple partition map entry %d: %w", block, err)
		}
		if binary.BigEndian.Uint16(entry) != partitionMapSignature {
			return fmt.Errorf("invalid Apple partition map signature in entry %d", block)
		}
		partitionType := string(entry[partitionTypeOffset : partitionTypeOffset+32])
		if strings.TrimRight(partitionType, "\x00") != "Apple_Driver43" {
			continue
		}
		if binary.BigEndian.Uint32(entry[12:]) < uint32((driverSize+diskBlockSize-1)/diskBlockSize) {
			return errors.New("MiniSCSI driver does not fit in its partition")
		}

		status := binary.BigEndian.Uint32(entry[partitionStatusOffset:])
		binary.BigEndian.PutUint32(entry[partitionStatusOffset:], status|partitionStatusInUse|partitionStatusHasBootInfo|partitionStatusBootCodePIC)
		// Keep hfdisk's "Driver_Partition" name. The Start Manager verifies
		// pmBootCksum only when the name starts with "Maci". MiniSCSI's
		// documented checksum description was insufficient to reproduce the
		// expected value, and verification prevented the driver from loading on
		// an SE/30. Leave verification disabled until its checksum algorithm is
		// specified or a known-good reference partition map is available.
		binary.BigEndian.PutUint32(entry[partitionBootStartOffset:], 0)
		binary.BigEndian.PutUint32(entry[partitionBootSizeOffset:], uint32(driverSize))
		binary.BigEndian.PutUint32(entry[partitionBootAddressOffset:], 0)
		binary.BigEndian.PutUint32(entry[partitionBootEntryOffset:], 0)
		// Retain the calculated value for diagnostics and future compatibility,
		// even though the partition name above intentionally disables validation.
		binary.BigEndian.PutUint32(entry[partitionBootChecksumOffset:], uint32(checksum))
		clear(entry[partitionProcessorOffset : partitionProcessorOffset+16])
		copy(entry[partitionProcessorOffset:partitionProcessorOffset+16], "68000")
		if _, err := image.WriteAt(entry, int64(block)*diskBlockSize); err != nil {
			return fmt.Errorf("write MiniSCSI partition map entry: %w", err)
		}
		return nil
	}
	return errors.New("Apple_Driver43 partition not found")
}

func miniSCSIChecksum(driver []byte) uint16 {
	var checksum uint32
	for len(driver) >= 2 {
		checksum += uint32(binary.BigEndian.Uint16(driver))
		driver = driver[2:]
	}
	if len(driver) == 1 {
		checksum += uint32(driver[0]) << 8
	}
	return uint16(checksum)
}

// creates an image and properties file pair
func (s *Server) handleDriveCreate(c *gin.Context) {
	fileName := c.PostForm("file_name")
	driveName := c.PostForm("drive_name")

	if fileName == "" || driveName == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name and drive name are required",
		})
		return
	}

	// Check if drive properties are loaded
	if s.driveProps == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Drive properties database not available",
		})
		return
	}

	// Get drive properties by name
	props, err := s.driveProps.GetByName(driveName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("No properties data for drive %s", driveName),
		})
		return
	}

	// Check that we have a valid file type and size
	if props.FileType == nil || *props.FileType == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Drive %s has no file type defined", driveName),
		})
		return
	}

	if props.Size == nil || *props.Size == 0 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Drive %s has no size defined", driveName),
		})
		return
	}

	// Create the full filename with extension
	fullFileName := fmt.Sprintf("%s.%s", fileName, *props.FileType)
	fullPath, propFilePath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fullFileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}
	propFileName := fmt.Sprintf("%s.properties", fullFileName)
	if _, err := os.Stat(propFilePath); err == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Properties file already exists: %s", propFileName),
		})
		return
	}

	// Use the exact capacity from the preset. Some drive and tape sizes are not
	// whole MiB values and were truncated by the former dd-based implementation.
	imageFile, err := os.OpenFile(fullPath, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o644)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to create image file: %v", err),
		})
		return
	}
	if err := imageFile.Truncate(*props.Size); err != nil {
		imageFile.Close()
		os.Remove(fullPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to size image file: %v", err),
		})
		return
	}
	if err := imageFile.Close(); err != nil {
		os.Remove(fullPath)
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to finish image file: %v", err),
		})
		return
	}

	// Build properties JSON
	if err := writeJSONAtomically(propFilePath, drivePropertyData(props)); err != nil {
		cleanupErr := os.Remove(fullPath)
		message := fmt.Sprintf("Failed to write properties file: %v", err)
		if cleanupErr != nil {
			message = fmt.Sprintf("%s; partial failure removing image: %v", message, cleanupErr)
		}
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: message,
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Image file with properties created: %s", fullFileName),
	})
}

func drivePropertyData(props *driveprops.DriveProperty) map[string]interface{} {
	return map[string]interface{}{
		"device_type": props.DeviceType,
		"vendor":      props.Vendor,
		"product":     props.Product,
		"revision":    props.Revision,
		"block_size":  props.BlockSize,
		"size":        props.Size,
		"name":        props.Name,
		"file_type":   props.FileType,
		"description": props.Description,
		"url":         props.URL,
	}
}

// creates a properties file for a CD-ROM image
func (s *Server) handleDriveCdrom(c *gin.Context) {
	fileName := c.PostForm("file_name")
	driveName := c.PostForm("drive_name")

	if fileName == "" || driveName == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "File name and drive name are required",
		})
		return
	}

	// Check if drive properties are loaded
	if s.driveProps == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Drive properties database not available",
		})
		return
	}

	// Get drive properties by name
	props, err := s.driveProps.GetByName(driveName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("No properties data for drive %s", driveName),
		})
		return
	}
	if props.DeviceType != pb.PbDeviceType_SCCD.String() {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Drive %s is not a CD/DVD profile", driveName),
		})
		return
	}

	// Create properties file for the image (without creating the image itself)
	propFileName := fmt.Sprintf("%s.properties", fileName)
	imagePath, propFilePath, err := imageAndPropertiesPaths(s.config.BaseDir, s.config.ConfigDir, fileName)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename",
		})
		return
	}
	if _, err := os.Stat(imagePath); err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("CD-ROM image not found: %s", fileName),
		})
		return
	}

	// Build properties JSON
	propData := map[string]interface{}{
		"device_type": props.DeviceType,
		"vendor":      props.Vendor,
		"product":     props.Product,
		"revision":    props.Revision,
		"block_size":  props.BlockSize,
		"size":        props.Size,
		"name":        props.Name,
		"file_type":   props.FileType,
		"description": props.Description,
		"url":         props.URL,
	}

	if err := writeJSONAtomically(propFilePath, propData); err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to write properties file: %v", err),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("Properties file created for CD-ROM image: %s", propFileName),
	})
}

// shows drive creation page
func (s *Server) handleDriveList(c *gin.Context) {
	// Get base template data
	data := s.getBaseTemplateData(c)
	data["Title"] = "PiSCSI - Create Drive"
	freeDiskSpace, err := freeDiskSpaceMiB(s.config.BaseDir)
	if err != nil {
		freeDiskSpace = 0
	}
	data["FreeDiskSpace"] = freeDiskSpace

	// Get list of available images
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ListImages(s.config.BaseDir))
	if err != nil {
		data["ErrorMessage"] = fmt.Sprintf("Failed to list images: %v", err)
		c.HTML(http.StatusOK, "drives.html", data)
		return
	}

	// Get drive properties and categorize by type.
	presets := drivePresetTemplateData(nil)
	if s.driveProps != nil {
		presets = drivePresetTemplateData(s.driveProps.GetAllDrives())
	}
	for category, categoryPresets := range presets {
		data[category] = categoryPresets
	}

	// Get list of existing CD-ROM image files
	data["CDImageFiles"] = compatibleCDImages(result.GetImageFilesInfo())

	c.HTML(http.StatusOK, "drives.html", data)
}

// shows system administration page
func (s *Server) handleSysAdmin(c *gin.Context) {
	// Get base template data
	data := s.getBaseTemplateData(c)
	data["Title"] = "PiSCSI - Settings"

	// Get server info for log levels
	cmdBuilder := s.getCommandBuilder(c)
	result, err := s.piscsiClient.SendCommand(cmdBuilder.ServerInfo())

	logLevels := []string{"trace", "debug", "info", "warning", "error"}
	currentLogLevel := "info"

	if err == nil && result.GetStatus() {
		serverInfo := result.GetServerInfo()
		if serverInfo != nil && serverInfo.GetLogLevelInfo() != nil {
			currentLogLevel = serverInfo.GetLogLevelInfo().GetCurrentLogLevel()
		}
	}

	data["LogLevels"] = logLevels
	data["CurrentLogLevel"] = currentLogLevel

	// Theme information
	themes := []string{"modern", "classic"}
	data["Themes"] = themes
	data["CurrentTheme"] = data["Theme"]

	type Locale struct {
		Language    string
		DisplayName string
	}
	locales := []Locale{
		{Language: "en", DisplayName: "English"},
		{Language: "de", DisplayName: "Deutsch"},
		{Language: "sv", DisplayName: "svenska"},
		{Language: "fr", DisplayName: "français"},
		{Language: "es", DisplayName: "español"},
		{Language: "zh", DisplayName: "中文"},
	}
	data["Locales"] = locales
	data["CurrentLocale"] = data["Locale"]

	services := s.companionServices()
	data["NetatalkConfigured"] = services.Netatalk
	data["SambaConfigured"] = services.Samba
	data["FtpConfigured"] = services.FTP
	data["MacproxyConfigured"] = services.Macproxy
	data["WebminConfigured"] = services.Webmin

	c.HTML(http.StatusOK, "admin.html", data)
}

// shows file upload page
func (s *Server) handleUploadPage(c *gin.Context) {
	// Get base template data
	data := s.getBaseTemplateData(c)
	data["Title"] = "PiSCSI - Upload"

	data["ConfigDir"] = s.config.ConfigDir
	s.addTransferDirectoryData(data)

	c.HTML(http.StatusOK, "upload.html", data)
}

func (s *Server) addTransferDirectoryData(data gin.H) {
	data["ImageDir"] = s.config.BaseDir
	data["ImageRootDir"] = s.config.BaseDir
	data["ImagesSubdirs"] = transferSubdirectories(s.config.BaseDir)

	sharedDirExists := directoryExists(s.config.SharedDir)
	data["FileServerDirExists"] = sharedDirExists
	data["SharedDir"] = s.config.SharedDir
	data["SharedRootDir"] = s.config.SharedDir
	if sharedDirExists {
		data["SharedSubdirs"] = transferSubdirectories(s.config.SharedDir)
	} else {
		data["SharedSubdirs"] = []string{}
	}
}

func directoryExists(name string) bool {
	info, err := os.Stat(name)
	return err == nil && info.IsDir()
}

func transferSubdirectories(root string) []string {
	subdirectories := []string{}
	_ = filepath.WalkDir(root, func(name string, entry os.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if !entry.IsDir() || name == root {
			return nil
		}
		if strings.HasPrefix(entry.Name(), ".") {
			return filepath.SkipDir
		}
		relativeName, err := filepath.Rel(root, name)
		if err == nil {
			subdirectories = append(subdirectories, relativeName)
		}
		return nil
	})
	sort.Strings(subdirectories)
	return subdirectories
}

// displays disk image information
func (s *Server) handleFilesDiskinfo(c *gin.Context) {
	fileName := c.PostForm("file_name")
	if fileName == "" {
		data := s.getBaseTemplateData(c)
		data["ErrorMessage"] = "File name is required"
		data["Title"] = "Error"
		c.HTML(http.StatusBadRequest, "diskinfo.html", data)
		return
	}

	fullPath, err := resolvePathWithin(s.config.BaseDir, fileName)
	if err != nil {
		data := s.getBaseTemplateData(c)
		data["ErrorMessage"] = "Invalid filename"
		data["Title"] = "Error"
		c.HTML(http.StatusBadRequest, "diskinfo.html", data)
		return
	}

	// Check if file exists
	_, err = os.Stat(fullPath)
	if os.IsNotExist(err) {
		data := s.getBaseTemplateData(c)
		data["ErrorMessage"] = fmt.Sprintf("File not found: %s", fileName)
		data["Title"] = "Error"
		c.HTML(http.StatusNotFound, "diskinfo.html", data)
		return
	}

	// disktype reports partition maps and filesystems as structured,
	// preformatted text.
	output, err := s.runSystemCommand("disktype", fullPath)
	diskInfo := string(output)
	if err != nil {
		diskInfo = "Unable to determine file type"
	}

	// Get base template data
	data := s.getBaseTemplateData(c)
	data["FileName"] = fileName
	data["DiskInfo"] = diskInfo
	data["Title"] = "PiSCSI Image Info"

	c.HTML(http.StatusOK, "diskinfo.html", data)
}

// displays the PiSCSI manual index and individual manual pages
func (s *Server) handleSysManpage(c *gin.Context) {
	app := c.Query("app")
	data := s.getBaseTemplateData(c)
	if app == "" {
		data["Manpages"] = piscsiManpages
		data["Title"] = "PiSCSI Manual Pages"
		c.HTML(http.StatusOK, "manpages.html", data)
		return
	}

	page, ok := findPiSCSIManpage(app)
	if !ok {
		data["ErrorMessage"] = fmt.Sprintf("%s is not a recognized PiSCSI app", app)
		data["Title"] = "Error"
		c.HTML(http.StatusBadRequest, "manpage.html", data)
		return
	}

	data["App"] = app
	data["Section"] = page.Section
	data["Title"] = fmt.Sprintf("Manual for %s(%d)", app, page.Section)

	manpagePath, err := findSystemManpage(app, page.Section, systemManpageDirs[page.Section])
	if err != nil {
		data["ErrorMessage"] = err.Error()
		c.HTML(http.StatusNotFound, "manpage.html", data)
		return
	}

	roff, err := readRoffManpage(manpagePath)
	if err != nil {
		data["ErrorMessage"] = fmt.Sprintf("Unable to read the %s manual page: %v", app, err)
		c.HTML(http.StatusInternalServerError, "manpage.html", data)
		return
	}

	manpageHTML, err := renderRoffManpage(c.Request.Context(), roff)
	if err != nil {
		data["ErrorMessage"] = fmt.Sprintf("Unable to render the %s manual page: %v", app, err)
		c.HTML(http.StatusInternalServerError, "manpage.html", data)
		return
	}

	data["Manpage"] = manpageHTML
	c.HTML(http.StatusOK, "manpage.html", data)
}

// Settings Endpoints

// changes the session language
func (s *Server) handleLanguage(c *gin.Context) {
	locale := c.PostForm("locale")
	if locale == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Locale is required",
		})
		return
	}
	if !web.IsSupportedLocale(locale) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "The requested locale is not supported.",
		})
		return
	}

	// Store locale in session
	session, err := s.getSession(c)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to get session: %v", err),
		})
		return
	}

	session.Values["language"] = locale
	if err := session.Save(c.Request, c.Writer); err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to save session: %v", err),
		})
		return
	}

	message := s.localizer.Translate(locale, "Changed Web Interface language to %(locale)s")
	message = strings.ReplaceAll(message, "%(locale)s", locale)
	s.respond(c, ResponseOptions{Message: message})
}

// handleTheme changes the UI theme
func (s *Server) handleTheme(c *gin.Context) {
	var theme string
	if c.Request.Method == "GET" {
		theme = c.Query("theme")
	} else {
		theme = c.PostForm("theme")
	}

	if theme == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Theme is required",
		})
		return
	}

	// Validate theme
	validThemes := map[string]bool{
		"modern":  true,
		"classic": true,
	}

	if !validThemes[theme] {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "The requested theme does not exist.",
		})
		return
	}

	// Store theme in session
	session, err := s.getSession(c)
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to get session: %v", err),
		})
		return
	}

	session.Values["theme"] = theme
	if err := session.Save(c.Request, c.Writer); err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to save session: %v", err),
		})
		return
	}

	locale := s.selectedLocale(c)
	message := fmt.Sprintf("Theme changed to '%s'.", theme)
	if locale != "en" {
		message = s.localizer.Translate(locale, "Theme changed to '%(theme)s'.")
		message = strings.ReplaceAll(message, "%(theme)s", theme)
	}
	s.respond(c, ResponseOptions{Message: message})
}

// handlePWA serves PWA resources
func (s *Server) handlePWA(c *gin.Context) {
	pwaPath, err := web.CleanPWAPath(c.Param("pwa_path"))
	if err != nil {
		c.String(http.StatusNotFound, "PWA resource not found")
		return
	}

	// Try embedded files first
	data, err := web.GetPWAFile(pwaPath)
	if err == nil {
		// Determine content type based on file extension
		contentType := "application/octet-stream"
		if strings.HasSuffix(pwaPath, ".json") {
			contentType = "application/json"
		} else if strings.HasSuffix(pwaPath, ".xml") {
			contentType = "application/xml"
		} else if strings.HasSuffix(pwaPath, ".png") {
			contentType = "image/png"
		} else if strings.HasSuffix(pwaPath, ".ico") {
			contentType = "image/x-icon"
		}
		c.Data(http.StatusOK, contentType, data)
		return
	}

	// Fallback to filesystem for development
	fullPath := filepath.Join(s.config.StaticDir, "pwa", pwaPath)

	// Check if file exists
	if _, err := os.Stat(fullPath); os.IsNotExist(err) {
		c.String(http.StatusNotFound, "PWA resource not found")
		return
	}

	c.File(fullPath)
}

// Advanced File Operations

// downloads a file from a URL to the selected configured directory
func (s *Server) handleFilesDownloadURL(c *gin.Context) {
	rawURL := strings.TrimSpace(c.PostForm("url"))
	destination := c.DefaultPostForm("destination", "disk_images")

	if rawURL == "" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "URL is required",
		})
		return
	}

	formValues := map[string]string{
		"destination":   destination,
		"images_subdir": c.PostForm("images_subdir"),
		"shared_subdir": c.PostForm("shared_subdir"),
	}
	if destination != "disk_images" && destination != "shared_files" {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Unknown destination",
		})
		return
	}
	destinationDir, err := s.uploadDestination(formValues)
	if err != nil || !directoryExists(destinationDir) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid destination directory",
		})
		return
	}

	parsedURL, err := neturl.ParseRequestURI(rawURL)
	if err != nil || (parsedURL.Scheme != "http" && parsedURL.Scheme != "https") || parsedURL.Host == "" {
		s.respond(c, ResponseOptions{Error: true, Message: "Invalid URL"})
		return
	}
	fileName, err := neturl.PathUnescape(pathpkg.Base(parsedURL.EscapedPath()))
	if err != nil || !isValidFilename(fileName) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "Invalid filename in URL",
		})
		return
	}

	fullPath := filepath.Join(destinationDir, fileName)
	if err := downloadURLToPath(c.Request.Context(), parsedURL.String(), fullPath); err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("Failed to download file: %v", err),
		})
		return
	}

	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("File downloaded successfully: %s", fileName),
	})
}

func downloadURLToPath(ctx context.Context, rawURL, destination string) error {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, rawURL, nil)
	if err != nil {
		return err
	}
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode < http.StatusOK || response.StatusCode >= http.StatusMultipleChoices {
		return fmt.Errorf("server returned %s", response.Status)
	}

	tempFile, err := os.CreateTemp(filepath.Dir(destination), ".piscsi-download-*")
	if err != nil {
		return err
	}
	tempName := tempFile.Name()
	defer os.Remove(tempName)

	if _, err = io.Copy(tempFile, response.Body); err != nil {
		tempFile.Close()
		return err
	}
	if err = tempFile.Sync(); err != nil {
		tempFile.Close()
		return err
	}
	if err = tempFile.Chmod(0644); err != nil {
		tempFile.Close()
		return err
	}
	if err = tempFile.Close(); err != nil {
		return err
	}
	return os.Rename(tempName, destination)
}

// create an image from either a downloaded URL or an existing image file.
func (s *Server) handleFilesCreateISO(c *gin.Context) {
	remoteURL := strings.TrimSpace(c.PostForm("url"))
	localFile := strings.TrimSpace(c.PostForm("file"))
	isoType := c.PostForm("type")

	isoArgs, ok := s.isoFormatArgs(isoType)
	if !ok {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("%s is not a valid CD-ROM format.", isoType),
		})
		return
	}
	if (remoteURL == "") == (localFile == "") {
		s.respond(c, ResponseOptions{Error: true, Message: "Specify either a URL or a local file."})
		return
	}

	var (
		isoPath    string
		sourcePath string
		cleanup    func()
		err        error
	)
	if remoteURL != "" {
		var fileName string
		sourcePath, fileName, cleanup, err = s.downloadISOSource(c.Request.Context(), remoteURL)
		if cleanup != nil {
			defer cleanup()
		}
		if err == nil {
			isoPath, err = resolvePathWithin(s.config.BaseDir, fileName+".iso")
		}
	} else {
		sourcePath, err = resolvePathWithin(s.config.BaseDir, localFile)
		if err == nil {
			isoPath, err = resolvePathWithin(s.config.BaseDir, localFile+".iso")
		}
	}
	if err != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("The following error occurred when creating the CD-ROM image: %v", err),
		})
		return
	}
	if info, statErr := os.Stat(sourcePath); statErr != nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("The following error occurred when creating the CD-ROM image: %v", statErr),
		})
		return
	} else if !info.Mode().IsRegular() && !info.IsDir() {
		s.respond(c, ResponseOptions{Error: true, Message: "The CD-ROM source is not a regular file or directory."})
		return
	}
	if _, statErr := os.Stat(isoPath); statErr == nil {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("The following error occurred when creating the CD-ROM image: destination %s already exists", filepath.Base(isoPath)),
		})
		return
	} else if !os.IsNotExist(statErr) {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("The following error occurred when creating the CD-ROM image: %v", statErr),
		})
		return
	}

	args := append(append([]string{}, isoArgs...), "-o", isoPath, sourcePath)
	output, err := exec.Command("genisoimage", args...).CombinedOutput()
	if err != nil {
		_ = os.Remove(isoPath)
		detail := strings.TrimSpace(string(output))
		if detail == "" {
			detail = err.Error()
		}
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: fmt.Sprintf("The following error occurred when creating the CD-ROM image: %s", detail),
		})
		return
	}

	isoName, err := filepath.Rel(s.config.BaseDir, isoPath)
	if err != nil {
		isoName = filepath.Base(isoPath)
	}
	isoName = filepath.ToSlash(isoName)
	s.respond(c, ResponseOptions{
		Message: fmt.Sprintf("CD-ROM image %s with type %s was created.", isoName, isoType),
	})
}

func (s *Server) isoFormatArgs(isoType string) ([]string, bool) {
	switch isoType {
	case "HFS":
		args := []string{"-hfs"}
		webDir := filepath.Dir(s.config.TemplatesDir)
		mapPaths := []string{
			filepath.Join(webDir, "genisoimage_hfs_resource_fork_map.txt"),
			filepath.Join(webDir, "..", "..", "..", "python", "web", "genisoimage_hfs_resource_fork_map.txt"),
		}
		for _, mapPath := range mapPaths {
			if info, err := os.Stat(mapPath); err == nil && info.Mode().IsRegular() {
				args = append(args, "-map", mapPath)
				return args, true
			}
		}
		if s.logger != nil {
			s.logger.Warn("genisoimage HFS resource fork map not found", "paths", mapPaths)
		}
		return args, true
	case "ISO-9660 Level 1":
		return []string{"-iso-level", "1"}, true
	case "ISO-9660 Level 2":
		return []string{"-iso-level", "2"}, true
	case "ISO-9660 Level 3":
		return []string{"-iso-level", "3"}, true
	case "Joliet":
		return []string{"-J"}, true
	case "Rock Ridge":
		return []string{"-r"}, true
	default:
		return nil, false
	}
}

func (s *Server) downloadISOSource(ctx context.Context, rawURL string) (string, string, func(), error) {
	parsed, err := neturl.ParseRequestURI(rawURL)
	if err != nil || (parsed.Scheme != "http" && parsed.Scheme != "https") || parsed.Host == "" {
		return "", "", nil, fmt.Errorf("invalid download URL")
	}
	fileName, err := neturl.PathUnescape(filepath.Base(parsed.Path))
	if err != nil || !isValidFilename(fileName) {
		return "", "", nil, fmt.Errorf("invalid filename in URL")
	}

	tempDir, err := os.MkdirTemp("", "piscsi-iso-*")
	if err != nil {
		return "", "", nil, fmt.Errorf("create temporary directory: %w", err)
	}
	cleanup := func() { _ = os.RemoveAll(tempDir) }
	downloadPath := filepath.Join(tempDir, fileName)

	request, err := http.NewRequestWithContext(ctx, http.MethodGet, parsed.String(), nil)
	if err != nil {
		cleanup()
		return "", "", nil, fmt.Errorf("prepare download: %w", err)
	}
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		cleanup()
		return "", "", nil, fmt.Errorf("download %s: %w", fileName, err)
	}
	defer response.Body.Close()
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		cleanup()
		return "", "", nil, fmt.Errorf("download %s: server returned %s", fileName, response.Status)
	}

	output, err := os.OpenFile(downloadPath, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
	if err != nil {
		cleanup()
		return "", "", nil, fmt.Errorf("create downloaded file: %w", err)
	}
	limit := s.config.MaxFileSize
	var reader io.Reader = response.Body
	if limit > 0 {
		reader = io.LimitReader(response.Body, limit+1)
	}
	written, copyErr := io.Copy(output, reader)
	closeErr := output.Close()
	if copyErr != nil || closeErr != nil {
		cleanup()
		if copyErr != nil {
			return "", "", nil, fmt.Errorf("download %s: %w", fileName, copyErr)
		}
		return "", "", nil, fmt.Errorf("finish download %s: %w", fileName, closeErr)
	}
	if limit > 0 && written > limit {
		cleanup()
		return "", "", nil, fmt.Errorf("download exceeds maximum file size")
	}

	if expanded, err := expandZipForISO(downloadPath, tempDir); err != nil {
		if s.logger != nil {
			s.logger.Warn("could not expand ZIP download; keeping the archive", "file", fileName, "error", err)
		}
	} else if expanded {
		_ = os.Remove(downloadPath)
	}
	return tempDir, fileName, cleanup, nil
}

func expandZipForISO(zipPath, destination string) (bool, error) {
	archive, err := zip.OpenReader(zipPath)
	if err != nil {
		return false, nil
	}
	defer archive.Close()

	for _, entry := range archive.File {
		if strings.Contains(entry.Name, "XtraStuf.mac") {
			return false, nil
		}
	}
	for _, entry := range archive.File {
		cleanName := filepath.Clean(filepath.FromSlash(entry.Name))
		if cleanName == "." || filepath.IsAbs(cleanName) || cleanName == ".." ||
			strings.HasPrefix(cleanName, ".."+string(filepath.Separator)) {
			return false, fmt.Errorf("unsafe ZIP member %q", entry.Name)
		}
		target := filepath.Join(destination, cleanName)
		if entry.FileInfo().IsDir() {
			if err := os.MkdirAll(target, 0o755); err != nil {
				return false, err
			}
			continue
		}
		if entry.Mode()&os.ModeSymlink != 0 {
			return false, fmt.Errorf("ZIP member %q is a symbolic link", entry.Name)
		}
		if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
			return false, err
		}
		source, err := entry.Open()
		if err != nil {
			return false, err
		}
		targetFile, err := os.OpenFile(target, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
		if err != nil {
			source.Close()
			return false, err
		}
		_, copyErr := io.Copy(targetFile, source)
		sourceErr := source.Close()
		targetErr := targetFile.Close()
		if copyErr != nil {
			return false, copyErr
		}
		if sourceErr != nil {
			return false, sourceErr
		}
		if targetErr != nil {
			return false, targetErr
		}
	}
	return true, nil
}

// handleFilesUploadForm is an alternative upload endpoint (similar to regular upload)
func (s *Server) handleFilesUploadForm(c *gin.Context) {
	// Reuse the existing upload handler logic
	s.handleFilesUpload(c)
}
