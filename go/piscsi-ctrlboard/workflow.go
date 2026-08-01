// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"fmt"
	"path/filepath"
	"sort"
	"sync/atomic"
	"time"

	"github.com/piscsi/piscsi/go/piscsi"
	"github.com/piscsi/piscsi/go/piscsi/configuration"
	pb "github.com/piscsi/piscsi/go/proto"
)

// ImageSelection carries the slot through the image picker so attach and
// insert use exactly the SCSI ID that opened the workflow.
type ImageSelection struct {
	Slot  SCSISlot
	Image *pb.PbImageFile
}

// SCSIWorkflow provides the daemon operations needed by the first interactive
// Control Board workflows. Its methods are synchronous by design; callers run
// them in a worker goroutine, never on the input event path.
type SCSIWorkflow struct {
	client     CommandSender
	commands   *piscsi.CommandBuilder
	diagnostic func(ImageListDiagnostic)
	configDir  string
	imageDir   string
}

// ImageListDiagnostic records the daemon response used to build the picker.
// It reports metadata rather than image filenames, keeping field diagnostics
// concise while still distinguishing an empty response from a failed request.
type ImageListDiagnostic struct {
	Status  bool
	Message string
	Folder  string
	Count   int
}

func NewSCSIWorkflow(client CommandSender, token string) *SCSIWorkflow {
	commands := piscsi.NewCommandBuilder()
	commands.SetToken(token)
	return &SCSIWorkflow{client: client, commands: commands}
}

// SetDiagnosticSink receives image-list response metadata. It is optional and
// intended for the command's existing --diagnostic mode.
func (w *SCSIWorkflow) SetDiagnosticSink(sink func(ImageListDiagnostic)) {
	if w != nil {
		w.diagnostic = sink
	}
}

// SetProfileDirectories configures the local object-style profile store and
// image root used for safe profile path resolution.
func (w *SCSIWorkflow) SetProfileDirectories(configDir, imageDir string) {
	if w == nil {
		return
	}
	w.configDir, w.imageDir = configDir, imageDir
}

func (w *SCSIWorkflow) BuildProfileMenu(pageSize int) (*Menu, error) {
	if w == nil {
		return nil, fmt.Errorf("SCSI workflow is not initialized")
	}
	return NewProfileMenu(w.configDir, pageSize)
}

func (w *SCSIWorkflow) LoadProfile(ctx context.Context, selection ProfileSelection) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	if w.configDir == "" || w.imageDir == "" {
		return "", fmt.Errorf("profile directories are not configured")
	}
	loader := configuration.Loader{ConfigDir: w.configDir, ImageDir: w.imageDir, Client: w.client, Commands: w.commands}
	if err := loader.Load(selection.Filename); err != nil {
		return "", fmt.Errorf("load profile %q: %w", selection.Filename, err)
	}
	return "Profile loaded", nil
}

// RunSystemCommand sends a privileged host-operation request through the
// daemon. The caller is responsible for displaying the status before calling
// this method, as a successful command may immediately end the process.
func (w *SCSIWorkflow) RunSystemCommand(ctx context.Context, selection SystemCommandSelection) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	message, err := selection.Kind.displayMessage()
	if err != nil {
		return "", err
	}
	// This worker delay lets the renderer present status before the daemon can
	// begin an operation that terminates the Control Board process.
	time.Sleep(500 * time.Millisecond)
	result, err := w.client.SendCommand(w.commands.ShutDown(string(selection.Kind)))
	if err != nil {
		return "", fmt.Errorf("send %s: %w", selection.Kind, err)
	}
	if !result.GetStatus() {
		return "", resultError(string(selection.Kind), result)
	}
	return message, nil
}

// BuildImageMenu requests the daemon's configured image folder using the same
// minimal request as the Python Control Board, then returns a name-sorted
// picker with a Return entry.
func (w *SCSIWorkflow) BuildImageMenu(ctx context.Context, slot SCSISlot, pageSize int) (*Menu, error) {
	if err := w.ready(ctx); err != nil {
		return nil, err
	}
	result, err := w.client.SendCommand(w.commands.ListDefaultImages())
	if err != nil {
		w.reportImageList(ImageListDiagnostic{Message: err.Error()})
		return nil, fmt.Errorf("list images: %w", err)
	}
	info := result.GetImageFilesInfo()
	w.reportImageList(ImageListDiagnostic{
		Status:  result.GetStatus(),
		Message: result.GetMsg(),
		Folder:  info.GetDefaultImageFolder(),
		Count:   len(info.GetImageFiles()),
	})
	if !result.GetStatus() {
		return nil, resultError("list images", result)
	}
	images := append([]*pb.PbImageFile(nil), info.GetImageFiles()...)
	sort.Slice(images, func(left, right int) bool { return images[left].GetName() < images[right].GetName() })
	items := make([]MenuItem, 0, len(images)+1)
	items = append(items, MenuItem{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}})
	for _, image := range images {
		if image == nil || image.GetName() == "" {
			continue
		}
		items = append(items, MenuItem{
			ID:    "image:" + image.GetName(),
			Label: fmt.Sprintf("%s [%s]", filepath.Base(image.GetName()), image.GetType()),
			Data:  ImageSelection{Slot: slot, Image: image},
		})
	}
	if len(items) == 1 {
		items = append(items, MenuItem{ID: "empty", Label: "(No image files found)"})
	}
	return NewMenu("Select Image", items, pageSize)
}

func (w *SCSIWorkflow) reportImageList(diagnostic ImageListDiagnostic) {
	if w != nil && w.diagnostic != nil {
		w.diagnostic(diagnostic)
	}
}

// AttachOrInsert attaches an image to an empty slot or inserts it into an
// existing compatible removable device with no media present.
func (w *SCSIWorkflow) AttachOrInsert(ctx context.Context, selection ImageSelection) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	if selection.Slot.Reserved {
		return "", fmt.Errorf("SCSI ID %d is reserved", selection.Slot.ID)
	}
	if selection.Image == nil || selection.Image.GetName() == "" || selection.Image.GetType() == pb.PbDeviceType_UNDEFINED {
		return "", fmt.Errorf("selected image has no supported device type")
	}
	command := w.commands.AttachDevice(selection.Slot.ID, 0, selection.Image.GetType(), selection.Image.GetName(), 0, nil)
	verb := "Attached"
	if device := selection.Slot.Device; device != nil && device.GetProperties().GetRemovable() && device.GetStatus().GetRemoved() {
		if device.GetType() != selection.Image.GetType() {
			return "", fmt.Errorf("cannot insert %s media into the existing %s device at SCSI ID %d", selection.Image.GetType(), device.GetType(), selection.Slot.ID)
		}
		command = w.commands.InsertMedia(selection.Slot.ID, device.GetUnit(), selection.Image.GetType(), selection.Image.GetName(), nil)
		verb = "Inserted"
	}
	result, err := w.client.SendCommand(command)
	if err != nil {
		return "", fmt.Errorf("%s image: %w", lower(verb), err)
	}
	if !result.GetStatus() {
		return "", resultError(lower(verb)+" image", result)
	}
	return fmt.Sprintf("%s ID %d", verb, selection.Slot.ID), nil
}

// DetachOrEject preserves the existing UI convention: removable media that is
// present is ejected; devices without media and non-removable devices detach.
func (w *SCSIWorkflow) DetachOrEject(ctx context.Context, slot SCSISlot) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	if slot.Reserved {
		return "", fmt.Errorf("SCSI ID %d is reserved", slot.ID)
	}
	if slot.Device == nil {
		return "", fmt.Errorf("SCSI ID %d is empty", slot.ID)
	}
	command := w.commands.DetachDevice(slot.ID, slot.Device.GetUnit())
	verb := "Detached"
	if slot.Device.GetProperties().GetRemovable() && !slot.Device.GetStatus().GetRemoved() && slot.Device.GetFile().GetName() != "" {
		command = w.commands.EjectDevice(slot.ID, slot.Device.GetUnit())
		verb = "Ejected"
	}
	result, err := w.client.SendCommand(command)
	if err != nil {
		return "", fmt.Errorf("%s device: %w", lower(verb), err)
	}
	if !result.GetStatus() {
		return "", resultError(lower(verb)+" device", result)
	}
	return fmt.Sprintf("%s ID %d", verb, slot.ID), nil
}

// Reserve prevents the selected SCSI ID from being used for a device.
func (w *SCSIWorkflow) Reserve(ctx context.Context, slot SCSISlot) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	if slot.Reserved {
		return "", fmt.Errorf("SCSI ID %d is already reserved", slot.ID)
	}
	ids, err := w.reservedIDs()
	if err != nil {
		return "", err
	}
	ids = append(ids, slot.ID)
	if err := w.setReservedIDs(ids); err != nil {
		return "", err
	}
	return fmt.Sprintf("Reserved ID %d", slot.ID), nil
}

// Release removes the reservation for the selected SCSI ID without changing
// any other reserved IDs.
func (w *SCSIWorkflow) Release(ctx context.Context, slot SCSISlot) (string, error) {
	if err := w.ready(ctx); err != nil {
		return "", err
	}
	if !slot.Reserved {
		return "", fmt.Errorf("SCSI ID %d is not reserved", slot.ID)
	}
	ids, err := w.reservedIDs()
	if err != nil {
		return "", err
	}
	remaining := make([]int32, 0, len(ids))
	found := false
	for _, id := range ids {
		if id == slot.ID {
			found = true
			continue
		}
		remaining = append(remaining, id)
	}
	if !found {
		return "", fmt.Errorf("SCSI ID %d is not reserved", slot.ID)
	}
	if err := w.setReservedIDs(remaining); err != nil {
		return "", err
	}
	return fmt.Sprintf("Released ID %d", slot.ID), nil
}

func (w *SCSIWorkflow) reservedIDs() ([]int32, error) {
	result, err := w.client.SendCommand(w.commands.ReservedIDsInfo())
	if err != nil {
		return nil, fmt.Errorf("get reserved IDs: %w", err)
	}
	if !result.GetStatus() {
		return nil, resultError("get reserved IDs", result)
	}
	return append([]int32(nil), result.GetReservedIdsInfo().GetIds()...), nil
}

func (w *SCSIWorkflow) setReservedIDs(ids []int32) error {
	result, err := w.client.SendCommand(w.commands.ReserveIDs(ids))
	if err != nil {
		return fmt.Errorf("set reserved IDs: %w", err)
	}
	if !result.GetStatus() {
		return resultError("set reserved IDs", result)
	}
	return nil
}

func (w *SCSIWorkflow) ready(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if w == nil || w.client == nil || w.commands == nil {
		return fmt.Errorf("SCSI workflow is not initialized")
	}
	return nil
}

func resultError(action string, result *pb.PbResult) error {
	if result == nil {
		return fmt.Errorf("%s: empty PiSCSI response", action)
	}
	if message := result.GetMsg(); message != "" {
		return fmt.Errorf("%s: %s", action, message)
	}
	return fmt.Errorf("%s failed", action)
}

func lower(value string) string {
	if value == "" {
		return value
	}
	return string(value[0]+('a'-'A')) + value[1:]
}

// WorkflowController connects menu selections to asynchronous daemon work.
// Its busy flag prevents a held or bouncing select switch from starting the
// same daemon operation more than once.
type WorkflowController struct {
	ctx      context.Context
	menu     *MenuController
	workflow *SCSIWorkflow
	pageSize int
	onError  func(error)
	busy     atomic.Bool
}

func NewWorkflowController(ctx context.Context, menu *MenuController, workflow *SCSIWorkflow, pageSize int, onError func(error)) (*WorkflowController, error) {
	if ctx == nil || menu == nil || workflow == nil || pageSize <= 0 {
		return nil, fmt.Errorf("context, menu, workflow, and page size are required")
	}
	return &WorkflowController{ctx: ctx, menu: menu, workflow: workflow, pageSize: pageSize, onError: onError}, nil
}

// Handle is safe to use as MenuController's selection callback.
func (c *WorkflowController) Handle(item MenuItem) {
	switch selected := item.Data.(type) {
	case SCSISlot:
		actions, err := NewSlotActionMenu(selected, c.pageSize)
		if err != nil {
			c.report(err)
			return
		}
		if err := c.menu.Push(actions); err != nil {
			c.report(err)
		}
	case SlotAction:
		switch selected.Kind {
		case SlotActionReturn:
			c.menu.Pop()
		case SlotActionAttachInsert:
			c.start("Loading images", func(ctx context.Context) (string, error) {
				images, err := c.workflow.BuildImageMenu(ctx, selected.Slot, c.pageSize)
				if err == nil {
					err = c.menu.Push(images)
				}
				return "", err
			})
		case SlotActionDetachEject:
			c.start("Working", func(ctx context.Context) (string, error) {
				return c.workflow.DetachOrEject(ctx, selected.Slot)
			})
		case SlotActionReserve:
			c.start("Working", func(ctx context.Context) (string, error) {
				return c.workflow.Reserve(ctx, selected.Slot)
			})
		case SlotActionRelease:
			c.start("Working", func(ctx context.Context) (string, error) {
				return c.workflow.Release(ctx, selected.Slot)
			})
		case SlotActionInfo:
			info, err := NewDeviceInfoMenu(selected.Slot, c.pageSize)
			if err != nil {
				c.report(err)
				return
			}
			if err := c.menu.Push(info); err != nil {
				c.report(err)
			}
		case SlotActionLoadProfile:
			c.start("Loading profiles", func(context.Context) (string, error) {
				profiles, err := c.workflow.BuildProfileMenu(c.pageSize)
				if err == nil {
					err = c.menu.Push(profiles)
				}
				return "", err
			})
		case SlotActionSystemInfo:
			c.start("Loading system", func(ctx context.Context) (string, error) {
				info, err := c.workflow.BuildSystemInfoMenu(ctx, selected.Slot, c.pageSize)
				if err == nil {
					err = c.menu.Push(info)
				}
				return "", err
			})
		case SlotActionSystemCmds:
			commands, err := NewSystemCommandsMenu(selected.Slot, c.pageSize)
			if err != nil {
				c.report(err)
				return
			}
			if err := c.menu.Push(commands); err != nil {
				c.report(err)
			}
		default:
			c.report(fmt.Errorf("%s is not implemented yet", selected.Kind))
		}
	case ImageSelection:
		c.start("Working", func(ctx context.Context) (string, error) {
			return c.workflow.AttachOrInsert(ctx, selected)
		})
	case ProfileSelection:
		c.start("Loading profile", func(ctx context.Context) (string, error) {
			return c.workflow.LoadProfile(ctx, selected)
		})
	case SystemCommandSelection:
		message, err := selected.Kind.displayMessage()
		if err != nil {
			c.report(err)
			return
		}
		c.start(message, func(ctx context.Context) (string, error) {
			return c.workflow.RunSystemCommand(ctx, selected)
		})
	}
}

func (c *WorkflowController) start(status string, operation func(context.Context) (string, error)) {
	if !c.busy.CompareAndSwap(false, true) {
		return
	}
	c.menu.ShowMessage(status, 30*time.Second)
	go func() {
		defer c.busy.Store(false)
		message, err := operation(c.ctx)
		if err != nil {
			c.report(err)
			c.menu.ShowMessage("Action failed", 1500*time.Millisecond)
			return
		}
		if message != "" {
			c.menu.ResetToRoot()
			c.menu.RequestRootRefresh()
			c.menu.ShowMessage(message, 1500*time.Millisecond)
			return
		}
		c.menu.ClearMessage()
	}()
}

func (c *WorkflowController) report(err error) {
	if c.onError != nil && err != nil {
		c.onError(err)
	}
}
