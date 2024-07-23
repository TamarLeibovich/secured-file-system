#pragma once

#define DRIVER_PREFIX "SecuredFileSystem: "
#define DRIVER_TAG 'trpD'



extern "C" {
	extern PFLT_FILTER filter;
	extern PFLT_PORT port;
	extern PFLT_PORT client_port;

	NTSTATUS InitMinifilter(PDRIVER_OBJECT driver_obj, PUNICODE_STRING registry_path);

	NTSTATUS PortConnectNotify(PFLT_PORT client_port, PVOID server_port_cookie, PVOID connection_context, ULONG size_of_context, PVOID*
		connection_port_cookie);

	void PortDisconnectNotify(PVOID connection_cookie);

	NTSTATUS PortMessageNotify(PVOID port_cookie, PVOID input_buffer, ULONG input_buffer_length, PVOID output_buffer, ULONG
		output_buffer_length, PULONG return_output_buffer_length);
}



