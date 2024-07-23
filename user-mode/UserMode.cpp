#include "C:\Python\include\Python.h"
#include <windows.h>
#include <stdio.h>
#include <fltUser.h>

#pragma comment(lib, "fltlib")

bool IsAccessAllowed();
bool UpdateRegistry(bool is_access_allowed);
void DisplayAccessDeniedWindow();

int main() {

	HANDLE hPort;

	// connecting to drivers's port
	auto hr = FilterConnectCommunicationPort(L"\\SecuredFileSystemPort", 0, nullptr, 0, nullptr, &hPort);

	if (FAILED(hr)) {

		printf("Error connecting to port\n");
		return EXIT_FAILURE;

	}

	// receiving message from driver
	BYTE buffer[1 << 12];	// 4 KB
	auto message = (FILTER_MESSAGE_HEADER*)buffer;

	while (true) {

		hr = FilterGetMessage(hPort, message, sizeof(buffer), nullptr);

		if (FAILED(hr)) {

			printf("Error receiving message, (0x%08X)\n", hr);
			break;

		}

		// preparing reply
		struct {
			FILTER_REPLY_HEADER reply_header;
			BOOLEAN is_access_allowed;
		} reply_message;

		reply_message.is_access_allowed = IsAccessAllowed();
		reply_message.reply_header.MessageId = message->MessageId;
		reply_message.reply_header.Status = 0;

		// set value in registry (0 -> access is denied, 1 -> access is approved)
		bool exit_status = UpdateRegistry(reply_message.is_access_allowed);

		if (exit_status == EXIT_FAILURE) {

			printf("failed updating registry key, exiting program\n");
			break;

		}

		// display access denied window if needed
		if (reply_message.is_access_allowed == 0)
			DisplayAccessDeniedWindow();

		// replying to driver
		hr = FilterReplyMessage(hPort, &reply_message.reply_header, sizeof(reply_message));

		if (FAILED(hr)) {

			printf("failed replying to message, error: (0x%08X)\n", hr);
			break;

		}


	}

	CloseHandle(hPort);

	return EXIT_SUCCESS;

}

bool IsAccessAllowed() {

	// initialize the python interpreter
	Py_Initialize();

	// add the directory to sys.path
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append('C:\\\\secured-file-system\\\\user-mode')");

	// get module's name
	PyObject* pName = PyUnicode_DecodeFSDefault("client");

	if (!pName) {

		printf("could not get the module`s name");
		Py_DECREF(pName);

		return false;

	}

	// get module
	PyObject* pModule = PyImport_Import(pName);

	if (!pModule) {

		printf("could not import module");
		Py_DECREF(pName);
		Py_DECREF(pModule);

		return false;

	}

	Py_DECREF(pName);

	// get the main function 
	PyObject* pFunction = PyObject_GetAttrString(pModule, "main");

	if (!pFunction) {

		printf("could not get function");
		Py_DECREF(pModule);
		Py_DECREF(pFunction);

		return false;

	}

	// call the main function and save the result in pResult variable
	PyObject* pResult = PyObject_CallObject(pFunction, NULL);

	// check the returned value
	if (!pResult || !PyBool_Check(pResult)) {

		printf("could not get return value");
		Py_DECREF(pModule);
		Py_DECREF(pFunction);
		Py_DECREF(pResult);

		return false;

	}

	bool return_value = Py_IsTrue(pResult);

	// destory objects
	Py_DECREF(pModule);
	Py_DECREF(pFunction);
	Py_DECREF(pResult);

	return return_value;
}

bool UpdateRegistry(bool is_access_allowed) {

	// this function modifies the Access entry in the registry to indicate the driver whether allow\deny access to file

	DWORD new_value = is_access_allowed ? 0 : 1;
	LPCWSTR subKey = L"SYSTEM\\CurrentControlSet\\Services\\SecuredFileSystem\\Instances\\SecuredFileSystemDefaultInstance";
	HKEY hKey;

	LONG status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_SET_VALUE, &hKey);

	if (status != ERROR_SUCCESS) {

		printf("error opening Access registry key, error: %ld\n", status);
		return EXIT_FAILURE;

	}

	status = RegSetValueExW(hKey, L"Access", 0, REG_DWORD, (const BYTE*)&new_value, sizeof(new_value));

	if (status != ERROR_SUCCESS) {

		printf("error setting value to Access key\n");
		return EXIT_FAILURE;

	}

	RegCloseKey(hKey);

	return EXIT_SUCCESS;
}

void DisplayAccessDeniedWindow() {

	// this function displays the access denied window by calling the module access_denied_window written in python

	// initialize the python interpreter
	Py_Initialize();

	// add the directory to sys.path
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append('C:\\\\secured-file-system\\\\user-mode')");

	// get module's name
	PyObject* pName = PyUnicode_DecodeFSDefault("access_denied_window");

	if (!pName) {

		printf("could not get the module`s name");
		Py_DECREF(pName);

	}

	// get module
	PyObject* pModule = PyImport_Import(pName);

	if (!pModule) {

		printf("could not import module");
		Py_DECREF(pName);
		Py_DECREF(pModule);

	}

	Py_DECREF(pName);

	// get the main function 
	PyObject* pFunction = PyObject_GetAttrString(pModule, "main");

	if (!pFunction) {

		printf("could not get function");
		Py_DECREF(pModule);
		Py_DECREF(pFunction);

	}

	// call the main function and save the result in pResult variable
	PyObject* pResult = PyObject_CallObject(pFunction, NULL);

	// destory objects
	Py_DECREF(pModule);
	Py_DECREF(pFunction);
	Py_DECREF(pResult);

}