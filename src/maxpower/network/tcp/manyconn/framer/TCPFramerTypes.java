/*********************************************************************
 * TCP Framer                                                        *
 * Copyright (C) 2013-2015 Maxeler Technologies                      *
 *                                                                   *
 * Author:  Itay Greenspon                                           *
 *                                                                   *
 *********************************************************************/

package maxpower.network.tcp.manyconn.framer;

import maxpower.network.tcp.manyconn.framer.TCPFramerConstants.FramerErrorCodes;
import maxpower.network.tcp.manyconn.framer.TCPFramerSM.FramerStates;

import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFETypeFactory;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.composite.DFEStructType;
import com.maxeler.maxcompiler.v2.statemachine.StateMachineLib;
import com.maxeler.maxcompiler.v2.statemachine.types.DFEsmValueType;
import com.maxeler.maxcompiler.v2.utils.MathUtils;
import com.maxeler.networking.types.TCPManyConnectionsTypes;
import com.maxeler.networking.v1.framed_kernels.FramedLinkType;

public class TCPFramerTypes {
	public static final DFEsmValueType lengthType = StateMachineLib.dfeUInt(MathUtils.bitsToRepresentUnsigned(100000));
	public final static DFEsmValueType maxMessageLengthType = StateMachineLib.dfeUInt(MathUtils.bitsToAddress(TCPFramerConstants.maxSupportedMessageLength));
	public final static DFEsmValueType levelRegType = StateMachineLib.dfeInt(MathUtils.bitsToRepresentSigned(TCPInterfaceTypes.maxWindowMemorySizeBytes));

	public static class ConnectionRecordType extends DFEStructType {
		public static final String HEADER = "header";
		public static final String HEADER_SIZE = "headerSize";
		public static final String LEVEL = "level";
		public static final String BYTES_NEEDED_AFTER_HEADER = "BytesNeededAfterHeader";
		public static final String LAST_RX_POS = "LastRxPos";
		public static final String CONNECTION_ERROR = "ConnectionError";
		public static final String HEADER_SEEN = "SeenHeader";

		public ConnectionRecordType(int maxHeaderSizeBytes) {
			super(
					sft(HEADER, DFETypeFactory.dfeRawBits(8*maxHeaderSizeBytes)),
					sft(HEADER_SIZE, DFETypeFactory.dfeUInt(MathUtils.bitsToRepresent(maxHeaderSizeBytes))),
					sft(LEVEL, DFETypeFactory.dfeInt(levelRegType.getTotalBits())),
					sft(BYTES_NEEDED_AFTER_HEADER, TCPInterfaceTypes.windowLevelBytesType),
					sft(LAST_RX_POS, DFETypeFactory.dfeUInt(3)),
					sft(CONNECTION_ERROR, DFETypeFactory.dfeBool()),
					sft(HEADER_SEEN, DFETypeFactory.dfeBool())
				);
		}
	}

	public static class EventStruct extends DFEStructType {
		public static final String SOCKET_ID = "socketID";
		public static final String IS_CONN_EVENT = "eventType";
		public static final String DATA = "Data";


		public EventStruct(TCPManyConnectionsTypes tcpRawType) {
			super(
					sft(SOCKET_ID, DFETypeFactory.dfeUInt(tcpRawType.getSocketSize())),
					sft(IS_CONN_EVENT, DFETypeFactory.dfeBool()),
					sft(DATA, DFETypeFactory.dfeUInt(32 - (tcpRawType.getSocketSize() + 1)))
				);
		}
	}

	public static class DebugStructType extends DFEStructType {
		public static final String EV_SOCKET_ID = "socketID";
		public static final String EV_DATA = "Data";
		public static final String RQ_SOCKET_ID = "RqSocketID";
		public static final String RQ_SIZE = "RqSize";

		public static final String EV_IS_CONN_EVENT = "eventType";
		public static final String EV_VALID = "evValid";
		public static final String RQ_VALID = "rqValid";
		public static final String PADDING = "padding";


		public DebugStructType() {
			super(
					sft(EV_SOCKET_ID, DFETypeFactory.dfeUInt(16)),
					sft(EV_DATA, DFETypeFactory.dfeUInt(16)),
					sft(RQ_SIZE, DFETypeFactory.dfeUInt(16)),
					sft(RQ_SOCKET_ID, DFETypeFactory.dfeUInt(16)),
					sft(EV_IS_CONN_EVENT, DFETypeFactory.dfeBool()),
					sft(EV_VALID, DFETypeFactory.dfeBool()),
					sft(RQ_VALID, DFETypeFactory.dfeBool()),
					sft(PADDING, DFETypeFactory.dfeUInt(64-3))
				);
		}
	}

	public static class TCPFramerLinkType extends DFEStructType implements FramedLinkType {
		public static final String EOF = "eof";
		public static final String SOF = "sof";
		public static final String MOD = "mod";
		public static final String DATA = "data";
		public static final String SOCKET = "socket";
		public static final String CONTAINS_DATA = "containsData";
		public static final String CONNECTION_STATE_VALID = "connectionStateValid";
		public static final String CONNECTION_STATE = "connectionState";
		public static final String ERROR_CODE = "errorCode";
		public static final String CURRENT_STATE = "currentState";
		public static final String LEVEL = "level";
		public static final String PROTOCOL_ID = "protocolID";
		public static final String IS_PASS_THROUGH = "isPassThrough";
		public static final int dataWidthBits = 64;

		public TCPFramerLinkType(TCPManyConnectionsTypes tcpRawType) {
			super(
					sft(DATA, DFETypeFactory.dfeRawBits(dataWidthBits)),
					sft(MOD, DFETypeFactory.dfeUInt(MathUtils.bitsToAddress(dataWidthBits / 8))),
					sft(SOF, DFETypeFactory.dfeBool()),
					sft(EOF, DFETypeFactory.dfeBool()),
					sft(SOCKET, DFETypeFactory.dfeUInt(tcpRawType.getSocketSize())),
					sft(CONTAINS_DATA, DFETypeFactory.dfeBool()),
					sft(CONNECTION_STATE_VALID, DFETypeFactory.dfeBool()),
					sft(CONNECTION_STATE, DFETypeFactory.dfeUInt(MathUtils.bitsToAddress(TCPInterfaceTypes.ConnectionStates.values().length))),
					sft(ERROR_CODE, DFETypeFactory.dfeUInt(MathUtils.bitsToAddress(FramerErrorCodes.values().length))),
					sft(CURRENT_STATE, DFETypeFactory.dfeUInt(MathUtils.bitsToAddress(FramerStates.values().length))),
					sft(LEVEL, DFETypeFactory.dfeInt(MathUtils.bitsToRepresentSigned(TCPInterfaceTypes.maxWindowMemorySizeBytes))),
					sft(PROTOCOL_ID, DFETypeFactory.dfeBool()),
					sft(IS_PASS_THROUGH, DFETypeFactory.dfeBool())
				);
		}

		@Override public String getEOF()  { return EOF; }
		@Override public String getSOF()  { return SOF; }
		@Override public String getData() { return DATA; }
		@Override public String getMod()  { return MOD; }
		@Override public DFEStructType getDFEStructType() { return this; }
	}
}
