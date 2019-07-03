package lc.kra.system;

public enum GlobalHookMode {
	/**
	 * capture events via the low-level system hook, after a event was captured any next hook registered will be called
	 */
	DEFAULT,
	/**
	 * capture events via the low-level system hook, this mode will not invoke any further hooks
	 */
	FINAL,
	/**
	 * capturing events in raw mode will provide you additional information of the device
	 */
	RAW
}
