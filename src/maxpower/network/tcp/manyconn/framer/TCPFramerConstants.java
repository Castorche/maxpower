/*********************************************************************
 * TCP Framer                                                        *
 * Copyright (C) 2013-2015 Maxeler Technologies                      *
 *                                                                   *
 * Author:  Itay Greenspon                                           *
 *                                                                   *
 *********************************************************************/

package maxpower.network.tcp.manyconn.framer;

import java.util.List;

import maxpower.network.tcp.manyconn.framer.TCPFramerSM.FramerStates;
import maxpower.network.tcp.manyconn.framer.proto.FramerProtocolSpec;

import com.maxeler.maxcompiler.v2.managers.DFEManager;
import com.maxeler.maxcompiler.v2.utils.MathUtils;

public class TCPFramerConstants {
	public static boolean enableDebug = false;
	public static boolean enableDebugPrints = false;
	public static boolean enableDebugStreams = false;
	public static boolean enableFramerDebugger = false;

	public static enum FramerErrorCodes {
		NoError,
		HeaderCorrupt,
		PayloadError,

		ShutdownDrain,
		BodyLengthTooBig,

		Reserved3,
		PayloadCutShort,

		PreviousErrors
	}

	public static final int maxSupportedMessageLength = 16 * 1024;
	public static final int modWidth = MathUtils.bitsToAddress(TCPInterfaceTypes.dataWordSizeBytes);
	public static final int outputBufferDepth = MathUtils.nextPowerOfTwo(2 * TCPFramerConstants.maxSupportedMessageLength / TCPInterfaceTypes.dataWordSizeBytes);
	public static final int outputBufferProgrammableFull = outputBufferDepth - (TCPFramerConstants.maxSupportedMessageLength / TCPInterfaceTypes.dataWordSizeBytes + 64 /* fudge factor */);
	public static final int outputEmptyLatency = 16;

	public static void addMaxfileConstants(DFEManager owner, List<FramerProtocolSpec> protoSpecs) {
		owner.addMaxFileConstant("framer_maxSupportedMessageLength", TCPFramerConstants.maxSupportedMessageLength);
		owner.addMaxFileConstant("framer_maxWindowMemorySizeBytes", TCPInterfaceTypes.maxWindowMemorySizeBytes);
		owner.addMaxFileConstant("framer_dataWordSizeBytes", TCPInterfaceTypes.dataWordSizeBytes);
		owner.addMaxFileConstant("framer_outputBufferDepth", TCPFramerConstants.outputBufferDepth);
		owner.addMaxFileConstant("framer_outputBufferProgrammableFull", TCPFramerConstants.outputBufferProgrammableFull);
		owner.addMaxFileConstant("framer_outputEmptyLatency", TCPFramerConstants.outputEmptyLatency);

		owner.addMaxFileConstant("framer_FixFramerConstants_enableDebugStreams", TCPFramerConstants.enableDebugStreams ? 1 : 0);

		owner.addMaxFileConstant("framer_FixFramerConstants_enableDebug", TCPFramerConstants.enableDebug ? 1 : 0);
		owner.addMaxFileConstant("framer_FixFramerConstants_enableFramerDebugger", TCPFramerConstants.enableFramerDebugger ? 1 : 0);

		for (FramerStates state : FramerStates.values()) {
			owner.addMaxFileConstant("framer_FramerStates_" + state.name(), state.ordinal());
		}

		for (FramerErrorCodes errorCode : FramerErrorCodes.values()) {
			owner.addMaxFileConstant("framer_FramerErrorCodes_" + errorCode.name(), errorCode.ordinal());
		}

		for (FramerProtocolSpec pSpec : protoSpecs) {
			owner.addMaxFileConstant("framer_ProtocolSpec_" + pSpec.getProtocolName(), protoSpecs.indexOf(pSpec));
			pSpec.addMaxfileConstants(owner);
		}

	}
}

