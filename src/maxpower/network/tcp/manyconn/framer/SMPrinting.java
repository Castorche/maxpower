/*********************************************************************
 * TCP Framer                                                        *
 * Copyright (C) 2013-2015 Maxeler Technologies                      *
 *                                                                   *
 * Author:  Itay Greenspon                                           *
 *                                                                   *
 *********************************************************************/

package maxpower.network.tcp.manyconn.framer;

import com.maxeler.maxcompiler.v2.statemachine.DFEsmStateEnum;
import com.maxeler.maxcompiler.v2.statemachine.DFEsmValue;
import com.maxeler.maxcompiler.v2.statemachine.StateMachineLib;

public interface SMPrinting {
	public StateMachineLib getOwner();
	public void printString(DFEsmValue string);
	public void printf(String sub, DFEsmStateEnum<?> currentStateSub, String format, Object ... args);

}
