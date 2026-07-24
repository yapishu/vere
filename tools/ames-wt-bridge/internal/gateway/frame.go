package gateway

import (
	"encoding/binary"
	"errors"
	"fmt"
	"net"
)

const (
	FrameSend byte = 1
	FrameHear byte = 2

	LaneGalaxy byte = 1
	LaneIPv4   byte = 2

	headerBytes = 8
)

var magic = [4]byte{'A', 'U', 'D', 'P'}

type Lane struct {
	Galaxy *byte
	IPv4   net.IP
	Port   uint16
}

type Frame struct {
	Type   byte
	Lane   Lane
	Packet []byte
}

func GalaxyLane(ship byte) Lane {
	return Lane{Galaxy: &ship}
}

func IPv4Lane(ip net.IP, port uint16) Lane {
	return Lane{IPv4: append(net.IP(nil), ip.To4()...), Port: port}
}

func Encode(typ byte, lane Lane, packet []byte) ([]byte, error) {
	if typ != FrameSend && typ != FrameHear {
		return nil, fmt.Errorf("unknown frame type %d", typ)
	}

	laneType := LaneIPv4
	laneLen := 6
	if lane.Galaxy != nil {
		laneType = LaneGalaxy
		laneLen = 1
	} else if lane.IPv4.To4() == nil || lane.Port == 0 {
		return nil, errors.New("invalid IPv4 UDP lane")
	}

	out := make([]byte, headerBytes+laneLen+len(packet))
	copy(out[:4], magic[:])
	out[4] = 1
	out[5] = typ
	out[6] = byte(laneType)
	out[7] = byte(laneLen)
	if lane.Galaxy != nil {
		out[headerBytes] = *lane.Galaxy
	} else {
		copy(out[headerBytes:headerBytes+4], lane.IPv4.To4())
		binary.BigEndian.PutUint16(out[headerBytes+4:headerBytes+6], lane.Port)
	}
	copy(out[headerBytes+laneLen:], packet)
	return out, nil
}

func Decode(input []byte) (Frame, error) {
	if len(input) < headerBytes {
		return Frame{}, errors.New("truncated UDP gateway frame")
	}
	if string(input[:4]) != string(magic[:]) {
		return Frame{}, errors.New("invalid UDP gateway frame magic")
	}
	if input[4] != 1 {
		return Frame{}, fmt.Errorf("unsupported UDP gateway frame version %d", input[4])
	}
	typ := input[5]
	if typ != FrameSend && typ != FrameHear {
		return Frame{}, fmt.Errorf("unknown UDP gateway frame type %d", typ)
	}
	laneType := input[6]
	laneLen := int(input[7])
	if len(input) < headerBytes+laneLen {
		return Frame{}, errors.New("truncated UDP gateway lane")
	}

	var lane Lane
	switch {
	case laneType == LaneGalaxy && laneLen == 1:
		lane = GalaxyLane(input[headerBytes])
	case laneType == LaneIPv4 && laneLen == 6:
		lane = IPv4Lane(
			net.IPv4(
				input[headerBytes],
				input[headerBytes+1],
				input[headerBytes+2],
				input[headerBytes+3],
			),
			binary.BigEndian.Uint16(input[headerBytes+4:headerBytes+6]),
		)
	default:
		return Frame{}, fmt.Errorf(
			"invalid UDP gateway lane type=%d length=%d",
			laneType,
			laneLen,
		)
	}

	return Frame{
		Type:   typ,
		Lane:   lane,
		Packet: append([]byte(nil), input[headerBytes+laneLen:]...),
	}, nil
}
