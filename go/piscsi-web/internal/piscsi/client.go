// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package piscsi

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"strconv"
	"time"

	pb "github.com/piscsi/piscsi-web/proto"
	"google.golang.org/protobuf/proto"
)

const (
	// Magic word sent at start of each message
	MagicWord = "RASCSI"

	// Default connection parameters
	DefaultHost    = "localhost"
	DefaultPort    = 6868
	DefaultTimeout = 10 * time.Second

	// Connection retry parameters
	MaxRetries    = 20
	RetryInterval = 200 * time.Millisecond

	// Read buffer size for receiving responses
	ReadBufferSize = 2048
)

var (
	ErrConnectionFailed = errors.New("failed to connect to piscsi daemon after retries")
	ErrInvalidResponse  = errors.New("invalid response from piscsi daemon")
	ErrEmptyResponse    = errors.New("empty response chunk from piscsi daemon")
	ErrInvalidMagicWord = errors.New("invalid magic word in response")
	ErrResponseTooLarge = errors.New("response size exceeds maximum allowed")
)

// Client represents a connection to the PiSCSI daemon
type Client struct {
	host    string
	port    int
	timeout time.Duration
}

// NewClient creates a new PiSCSI client
func NewClient(host string, port int) *Client {
	if host == "" {
		host = DefaultHost
	}
	if port == 0 {
		port = DefaultPort
	}

	return &Client{
		host:    host,
		port:    port,
		timeout: DefaultTimeout,
	}
}

// SetTimeout sets the connection timeout
func (c *Client) SetTimeout(timeout time.Duration) {
	c.timeout = timeout
}

// SendCommand sends a protobuf command to the piscsi daemon and returns the response
func (c *Client) SendCommand(cmd *pb.PbCommand) (*pb.PbResult, error) {
	// Serialize the command to protobuf
	payload, err := proto.Marshal(cmd)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal command: %w", err)
	}

	// Connect with retries
	conn, err := c.connectWithRetry()
	if err != nil {
		return nil, err
	}
	defer conn.Close()

	// Set deadline for the entire operation
	if err := conn.SetDeadline(time.Now().Add(c.timeout)); err != nil {
		return nil, fmt.Errorf("failed to set deadline: %w", err)
	}

	// Send the request
	if err := c.sendRequest(conn, payload); err != nil {
		return nil, err
	}

	// Receive the response
	responseData, err := c.receiveResponse(conn)
	if err != nil {
		return nil, err
	}

	// Unmarshal the response
	result := &pb.PbResult{}
	if err := proto.Unmarshal(responseData, result); err != nil {
		return nil, fmt.Errorf("failed to unmarshal response: %w", err)
	}

	return result, nil
}

// connectWithRetry attempts to connect to the piscsi daemon with retries
func (c *Client) connectWithRetry() (net.Conn, error) {
	address := net.JoinHostPort(c.host, strconv.Itoa(c.port))

	var conn net.Conn
	var err error

	for i := 0; i < MaxRetries; i++ {
		conn, err = net.DialTimeout("tcp", address, c.timeout)
		if err == nil {
			return conn, nil
		}

		// Wait before retrying
		if i < MaxRetries-1 {
			time.Sleep(RetryInterval)
		}
	}

	return nil, fmt.Errorf("%w: %v", ErrConnectionFailed, err)
}

// sendRequest sends the magic word, size header, and payload
func (c *Client) sendRequest(conn net.Conn, payload []byte) error {
	// Send magic word (6 bytes: "RASCSI")
	if _, err := conn.Write([]byte(MagicWord)); err != nil {
		return fmt.Errorf("failed to send magic word: %w", err)
	}

	// Send payload size (4 bytes, little-endian)
	sizeHeader := make([]byte, 4)
	binary.LittleEndian.PutUint32(sizeHeader, uint32(len(payload)))
	if _, err := conn.Write(sizeHeader); err != nil {
		return fmt.Errorf("failed to send size header: %w", err)
	}

	// Send payload
	if _, err := conn.Write(payload); err != nil {
		return fmt.Errorf("failed to send payload: %w", err)
	}

	return nil
}

// receiveResponse receives the response from the piscsi daemon
func (c *Client) receiveResponse(conn net.Conn) ([]byte, error) {
	// Read the size header (4 bytes, little-endian)
	sizeHeader := make([]byte, 4)
	if _, err := io.ReadFull(conn, sizeHeader); err != nil {
		return nil, fmt.Errorf("failed to read size header: %w", err)
	}

	responseSize := binary.LittleEndian.Uint32(sizeHeader)

	// Sanity check: response shouldn't be larger than 10MB
	if responseSize > 10*1024*1024 {
		return nil, fmt.Errorf("%w: %d bytes", ErrResponseTooLarge, responseSize)
	}

	// Read the response payload in chunks
	response := make([]byte, 0, responseSize)
	buffer := make([]byte, ReadBufferSize)

	for uint32(len(response)) < responseSize {
		n, err := conn.Read(buffer)
		if err != nil {
			if err == io.EOF && uint32(len(response)) < responseSize {
				return nil, ErrEmptyResponse
			}
			if err != io.EOF {
				return nil, fmt.Errorf("failed to read response: %w", err)
			}
		}

		if n == 0 {
			return nil, ErrEmptyResponse
		}

		response = append(response, buffer[:n]...)
	}

	return response, nil
}

// Address returns the connection address
func (c *Client) Address() string {
	return fmt.Sprintf("%s:%d", c.host, c.port)
}
