/*********************************************************************
 * TCP Framer                                                        *
 * Copyright (C) 2013-2015 Maxeler Technologies                      *
 *                                                                   *
 * Author:  Itay Greenspon                                           *
 *                                                                   *
 *********************************************************************/

package maxpower.network.tcp.manyconn.framer.proto;

import maxpower.network.tcp.manyconn.framer.TCPFramerSM;

import com.maxeler.maxcompiler.v2.managers.DFEManager;
import com.maxeler.maxcompiler.v2.statemachine.DFEsmValue;
import com.maxeler.maxcompiler.v2.statemachine.StateMachineLib;

public abstract class FramerProtocolSpec extends StateMachineLib {
	protected FramerProtocolSpec(TCPFramerSM owner) {
		super(owner);
	}

	public abstract DFEsmValue verifySignature(DFEsmValue stagingReg);
	public abstract DFEsmValue decodeMessageLength(DFEsmValue decodedLength);
	public abstract DFEsmValue validateLength(DFEsmValue stagingReg);
	public abstract int getMinimumHeaderSizeBytes();
	public abstract String getProtocolName();
	public abstract void addMaxfileConstants(DFEManager owner);


}
