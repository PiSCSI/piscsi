// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"archive/tar"
	"archive/zip"
	"compress/gzip"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
)

var archiveSuffixes = []string{"zip", "sit", "tar", "gz", "7z"}

type archiveMember struct {
	Name                  string
	Path                  string
	Size                  int64
	IsPropertiesFile      bool
	RelatedPropertiesFile string
}

type archiveCacheEntry struct {
	Size       int64
	ModifiedAt time.Time
	Members    []archiveMember
}

type lsarResult struct {
	Contents []lsarMember `json:"lsarContents"`
}

type lsarMember struct {
	Name           string        `json:"XADFileName"`
	Size           flexibleInt64 `json:"XADFileSize"`
	IsDirectory    flexibleBool  `json:"XADIsDirectory"`
	IsResourceFork flexibleBool  `json:"XADIsResourceFork"`
	IsLink         flexibleBool  `json:"XADIsLink"`
}

type flexibleBool bool

func (value *flexibleBool) UnmarshalJSON(data []byte) error {
	var boolean bool
	if err := json.Unmarshal(data, &boolean); err == nil {
		*value = flexibleBool(boolean)
		return nil
	}
	var number json.Number
	if err := json.Unmarshal(data, &number); err == nil {
		parsed, err := strconv.ParseInt(number.String(), 10, 64)
		if err == nil {
			*value = parsed != 0
			return nil
		}
	}
	var text string
	if err := json.Unmarshal(data, &text); err == nil {
		parsed, err := strconv.ParseBool(text)
		if err == nil {
			*value = flexibleBool(parsed)
			return nil
		}
	}
	return fmt.Errorf("invalid boolean value %q", data)
}

type flexibleInt64 int64

func (value *flexibleInt64) UnmarshalJSON(data []byte) error {
	if string(data) == "null" {
		*value = 0
		return nil
	}
	var number json.Number
	if err := json.Unmarshal(data, &number); err == nil {
		parsed, err := strconv.ParseInt(number.String(), 10, 64)
		if err == nil {
			*value = flexibleInt64(parsed)
			return nil
		}
	}
	var text string
	if err := json.Unmarshal(data, &text); err == nil {
		parsed, err := strconv.ParseInt(text, 10, 64)
		if err == nil {
			*value = flexibleInt64(parsed)
			return nil
		}
	}
	return fmt.Errorf("invalid integer value %q", data)
}

func isArchiveSuffix(suffix string) bool {
	suffix = strings.ToLower(strings.TrimPrefix(suffix, "."))
	for _, supported := range archiveSuffixes {
		if suffix == supported {
			return true
		}
	}
	return false
}

func cloneArchiveMembers(members []archiveMember) []archiveMember {
	return append([]archiveMember(nil), members...)
}

// inspectArchive returns displayable archive members. Results are cached until
// either the archive size or modification timestamp changes.
func (s *Server) inspectArchive(path string, info os.FileInfo) ([]archiveMember, error) {
	s.archiveMu.Lock()
	if cached, ok := s.archiveCache[path]; ok &&
		cached.Size == info.Size() && cached.ModifiedAt.Equal(info.ModTime()) {
		members := cloneArchiveMembers(cached.Members)
		s.archiveMu.Unlock()
		return members, nil
	}
	s.archiveMu.Unlock()

	rawMembers, err := inspectArchiveMembers(path)
	if err != nil {
		return nil, err
	}

	properties := make(map[string]struct{})
	for _, member := range rawMembers {
		if bool(member.IsDirectory) || bool(member.IsResourceFork) || bool(member.IsLink) || !safeArchiveMemberPath(member.Name) {
			continue
		}
		if strings.EqualFold(filepath.Ext(member.Name), ".properties") {
			properties[filepath.ToSlash(member.Name)] = struct{}{}
		}
	}

	members := make([]archiveMember, 0, len(rawMembers))
	for _, item := range rawMembers {
		if bool(item.IsDirectory) || bool(item.IsResourceFork) || bool(item.IsLink) || !safeArchiveMemberPath(item.Name) {
			continue
		}
		memberPath := filepath.ToSlash(item.Name)
		member := archiveMember{
			Name: filepath.Base(filepath.FromSlash(memberPath)),
			Path: memberPath,
			Size: int64(item.Size),
		}
		if strings.EqualFold(filepath.Ext(memberPath), ".properties") {
			member.IsPropertiesFile = true
		} else if _, ok := properties[memberPath+".properties"]; ok {
			member.RelatedPropertiesFile = memberPath + ".properties"
		}
		members = append(members, member)
	}
	sort.Slice(members, func(i, j int) bool { return members[i].Path < members[j].Path })

	s.archiveMu.Lock()
	if s.archiveCache == nil {
		s.archiveCache = make(map[string]archiveCacheEntry)
	}
	s.archiveCache[path] = archiveCacheEntry{
		Size:       info.Size(),
		ModifiedAt: info.ModTime(),
		Members:    cloneArchiveMembers(members),
	}
	s.archiveMu.Unlock()
	return members, nil
}

func inspectArchiveMembers(path string) ([]lsarMember, error) {
	var lsarErr error
	if _, err := exec.LookPath("lsar"); err == nil {
		output, commandErr := exec.Command("lsar", "-json", "--", path).Output()
		if commandErr == nil {
			var result lsarResult
			if jsonErr := json.Unmarshal(output, &result); jsonErr == nil {
				return result.Contents, nil
			} else {
				lsarErr = fmt.Errorf("parse lsar output: %w", jsonErr)
			}
		} else {
			lsarErr = fmt.Errorf("inspect archive with lsar: %w", commandErr)
		}
	} else {
		lsarErr = fmt.Errorf("lsar is unavailable")
	}

	var members []lsarMember
	var fallbackErr error
	switch strings.ToLower(filepath.Ext(path)) {
	case ".zip":
		members, fallbackErr = inspectZip(path)
	case ".tar":
		members, fallbackErr = inspectTar(path)
	case ".gz":
		members, fallbackErr = inspectGzip(path)
	default:
		members, fallbackErr = inspectWithBSDTar(path)
	}
	if fallbackErr == nil {
		return members, nil
	}
	return nil, fmt.Errorf("%v; fallback inspection failed: %w", lsarErr, fallbackErr)
}

func inspectZip(path string) ([]lsarMember, error) {
	reader, err := zip.OpenReader(path)
	if err != nil {
		return nil, err
	}
	defer reader.Close()
	members := make([]lsarMember, 0, len(reader.File))
	for _, file := range reader.File {
		members = append(members, lsarMember{
			Name:        file.Name,
			Size:        flexibleInt64(file.UncompressedSize64),
			IsDirectory: flexibleBool(file.FileInfo().IsDir()),
			IsLink:      flexibleBool(file.Mode()&os.ModeSymlink != 0),
		})
	}
	return members, nil
}

func inspectTar(path string) ([]lsarMember, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	return inspectTarReader(tar.NewReader(file))
}

func inspectTarReader(reader *tar.Reader) ([]lsarMember, error) {
	var members []lsarMember
	for {
		header, err := reader.Next()
		if err == io.EOF {
			return members, nil
		}
		if err != nil {
			return nil, err
		}
		members = append(members, lsarMember{
			Name:        header.Name,
			Size:        flexibleInt64(header.Size),
			IsDirectory: flexibleBool(header.FileInfo().IsDir()),
			IsLink:      flexibleBool(header.Typeflag == tar.TypeSymlink || header.Typeflag == tar.TypeLink),
		})
	}
}

func inspectGzip(path string) ([]lsarMember, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	reader, err := gzip.NewReader(file)
	if err != nil {
		return nil, err
	}
	defer reader.Close()

	if members, tarErr := inspectTarReader(tar.NewReader(reader)); tarErr == nil && len(members) > 0 {
		return members, nil
	}
	name := reader.Name
	if name == "" {
		name = strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
	}
	return []lsarMember{{Name: name}}, nil
}

func inspectWithBSDTar(path string) ([]lsarMember, error) {
	if _, err := exec.LookPath("bsdtar"); err != nil {
		return nil, fmt.Errorf("bsdtar is unavailable")
	}
	output, err := exec.Command("bsdtar", "-tf", path).Output()
	if err != nil {
		return nil, err
	}
	var members []lsarMember
	for _, name := range strings.Split(strings.TrimSpace(string(output)), "\n") {
		if name != "" {
			members = append(members, lsarMember{Name: name, IsDirectory: flexibleBool(strings.HasSuffix(name, "/"))})
		}
	}
	return members, nil
}

func safeArchiveMemberPath(name string) bool {
	if strings.TrimSpace(name) == "" {
		return false
	}
	cleaned := filepath.Clean(filepath.FromSlash(name))
	return cleaned != "." && cleaned != ".." && !filepath.IsAbs(cleaned) &&
		!strings.HasPrefix(cleaned, ".."+string(filepath.Separator))
}

func requestedArchiveMembers(raw string, available []archiveMember) ([]archiveMember, error) {
	if strings.TrimSpace(raw) == "" {
		return nil, fmt.Errorf("no files were specified")
	}
	byPath := make(map[string]archiveMember, len(available))
	for _, member := range available {
		byPath[member.Path] = member
	}

	result := make([]archiveMember, 0)
	seen := make(map[string]struct{})
	for _, requested := range strings.Split(raw, "|") {
		if _, duplicate := seen[requested]; duplicate {
			continue
		}
		member, ok := byPath[requested]
		if !ok {
			return nil, fmt.Errorf("archive member %q was not found", requested)
		}
		seen[requested] = struct{}{}
		result = append(result, member)
	}
	return result, nil
}

// moveExtractedFile preserves rename semantics while also supporting image or
// properties directories mounted on a different filesystem than /tmp.
func moveExtractedFile(source, target string) error {
	if err := os.Rename(source, target); err == nil {
		return nil
	} else if !errors.Is(err, syscall.EXDEV) {
		return err
	}

	input, err := os.Open(source)
	if err != nil {
		return err
	}
	defer input.Close()
	info, err := input.Stat()
	if err != nil {
		return err
	}
	output, err := os.CreateTemp(filepath.Dir(target), "."+filepath.Base(target)+".extract-*")
	if err != nil {
		return err
	}
	tempPath := output.Name()
	defer os.Remove(tempPath)
	if err := output.Chmod(info.Mode().Perm()); err != nil {
		output.Close()
		return err
	}
	if _, err := io.Copy(output, input); err != nil {
		output.Close()
		return err
	}
	if err := output.Close(); err != nil {
		return err
	}
	if err := os.Rename(tempPath, target); err != nil {
		return err
	}
	return os.Remove(source)
}

// handleFilesExtractImage extracts selected members directly beneath BaseDir.
// This avoids needing temporary storage as large as the archive contents. Files
// left behind by a failed extraction are deliberately retained in BaseDir so
// that they are visible and can be removed through the web interface.
func (s *Server) handleFilesExtractImage(c *gin.Context) {
	archiveName := c.PostForm("archive_file")
	archivePath, err := resolvePathWithin(s.config.BaseDir, archiveName)
	if err != nil || !isArchiveSuffix(filepath.Ext(archiveName)) {
		s.respond(c, ResponseOptions{Error: true, Message: "Invalid archive file name"})
		return
	}
	info, err := os.Stat(archivePath)
	if err != nil || !info.Mode().IsRegular() {
		s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Archive file not found: %s", archiveName)})
		return
	}

	available, err := s.inspectArchive(archivePath, info)
	if err != nil {
		s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Unable to inspect archive: %v", err)})
		return
	}
	requested, err := requestedArchiveMembers(c.PostForm("archive_members"), available)
	if err != nil {
		s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Unable to extract archive: %v", err)})
		return
	}

	toExtract := make([]archiveMember, 0, len(requested))
	skipped := make([]string, 0)
	for _, member := range requested {
		if member.IsPropertiesFile {
			toExtract = append(toExtract, member)
			continue
		}
		target, targetErr := resolvePathWithin(s.config.BaseDir, member.Path)
		if targetErr != nil {
			s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Unable to extract archive: %v", targetErr)})
			return
		}
		if _, statErr := os.Stat(target); statErr == nil {
			skipped = append(skipped, member.Path)
			continue
		} else if !os.IsNotExist(statErr) {
			s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Unable to extract archive: %v", statErr)})
			return
		}
		toExtract = append(toExtract, member)
	}

	if len(toExtract) > 0 {
		if output, err := extractArchiveMembers(archivePath, s.config.BaseDir, toExtract); err != nil {
			detail := strings.TrimSpace(string(output))
			if detail != "" {
				err = fmt.Errorf("%w: %s", err, detail)
			}
			s.respond(c, ResponseOptions{Error: true, Message: fmt.Sprintf("Unable to extract archive: %v", err)})
			return
		}
	}

	extracted := make([]string, 0, len(toExtract))
	failed := make([]string, 0)
	for _, member := range toExtract {
		source, sourceErr := resolvePathWithin(s.config.BaseDir, member.Path)
		if sourceErr != nil {
			failed = append(failed, member.Path)
			continue
		}
		if sourceInfo, statErr := os.Stat(source); statErr != nil || !sourceInfo.Mode().IsRegular() {
			failed = append(failed, member.Path)
			continue
		}

		if member.IsPropertiesFile {
			target, targetErr := resolvePathWithin(s.config.ConfigDir, member.Path)
			if targetErr != nil {
				failed = append(failed, member.Path)
				continue
			}
			if mkdirErr := os.MkdirAll(filepath.Dir(target), 0o755); mkdirErr != nil {
				failed = append(failed, member.Path)
				continue
			}
			if renameErr := moveExtractedFile(source, target); renameErr != nil {
				failed = append(failed, member.Path)
				continue
			}
		}
		extracted = append(extracted, member.Path)
	}

	if len(failed) > 0 {
		s.respond(c, ResponseOptions{
			Error: true,
			Message: fmt.Sprintf(
				"Extracted %d file(s), skipped %d existing file(s), and failed to extract %d file(s)",
				len(extracted), len(skipped), len(failed),
			),
		})
		return
	}
	if len(extracted) == 0 {
		s.respond(c, ResponseOptions{
			Error:   true,
			Message: "No files were extracted (existing files are skipped)",
		})
		return
	}
	message := fmt.Sprintf("Extracted %d file(s)", len(extracted))
	if len(skipped) > 0 {
		message += fmt.Sprintf("; skipped %d existing file(s)", len(skipped))
	}
	s.respond(c, ResponseOptions{
		Message: message,
	})
}

func extractArchiveMembers(archivePath, outputDir string, members []archiveMember) ([]byte, error) {
	if _, err := exec.LookPath("unar"); err == nil {
		args := []string{"-output-directory", outputDir, "-force-skip", "-no-directory", "-forks", "visible", "--", archivePath}
		for _, member := range members {
			args = append(args, regexp.QuoteMeta(member.Path))
		}
		return exec.Command("unar", args...).CombinedOutput()
	}
	if _, err := exec.LookPath("bsdtar"); err != nil {
		return nil, fmt.Errorf("neither unar nor bsdtar is available")
	}
	args := []string{"-xf", archivePath, "-C", outputDir, "--"}
	for _, member := range members {
		args = append(args, member.Path)
	}
	return exec.Command("bsdtar", args...).CombinedOutput()
}
