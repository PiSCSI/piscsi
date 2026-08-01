package testutil

import (
	pb "github.com/piscsi/piscsi/go/proto"
)

// MockPiSCSIClient is a mock PiSCSI client for testing
type MockPiSCSIClient struct {
	SendCommandFunc func(cmd *pb.PbCommand) (*pb.PbResult, error)
}

// SendCommand calls the mock function
func (m *MockPiSCSIClient) SendCommand(cmd *pb.PbCommand) (*pb.PbResult, error) {
	if m.SendCommandFunc != nil {
		return m.SendCommandFunc(cmd)
	}
	// Default: return success
	return &pb.PbResult{
		Status: true,
		Msg:    "OK",
	}, nil
}

// NewMockPiSCSIClient creates a new mock PiSCSI client
func NewMockPiSCSIClient() *MockPiSCSIClient {
	return &MockPiSCSIClient{}
}

// NewMockPiSCSIClientAlwaysSuccess creates a mock that always succeeds
func NewMockPiSCSIClientAlwaysSuccess() *MockPiSCSIClient {
	return &MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			return &pb.PbResult{
				Status: true,
				Msg:    "Operation successful",
			}, nil
		},
	}
}

// NewMockPiSCSIClientAlwaysFail creates a mock that always fails
func NewMockPiSCSIClientAlwaysFail(errorMsg string) *MockPiSCSIClient {
	return &MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			return &pb.PbResult{
				Status: false,
				Msg:    errorMsg,
			}, nil
		},
	}
}

// NewMockPiSCSIClientWithError creates a mock that returns an error
func NewMockPiSCSIClientWithError(err error) *MockPiSCSIClient {
	return &MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			return nil, err
		},
	}
}
