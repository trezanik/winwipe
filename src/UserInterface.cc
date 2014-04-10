
/**
 * @file	UserInterface.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <Windows.h>		// OS API

#include "app.h"
#include "UserInterface.h"
#include "utils.h"
#include "Runtime.h"
#include "Configuration.h"
#include "Log.h"



void
UserInterface::Output(
	DWORD text_color,
	char* text_format,
	...
)
{
	//Log*		log = runtime.Logger();
	char		output_mb[1024];
	wchar_t		output_text[1024];
	va_list		varg;


	va_start(varg, text_format);
	// vsnprintf with size-1 would be perfectly safe...
#if MSVC_IS_VS10_OR_LATER	// changed to 4 args since VS2010
	vsnprintf_s(output_mb, sizeof(output_mb), text_format, varg);
#else
	vsnprintf_s(output_mb, sizeof(output_mb), _TRUNCATE, text_format, varg);
#endif
	va_end(varg);


	MbToUTF8(output_text, output_mb, _countof(output_text));

	// output text is white in colour by default (NULL given as colour <black>)
	_cf.crTextColor  = text_color == NULL ? OUTCOLOR_NORMAL : text_color;
	
	// output formatting obtained; now do the actual output

#if 0	// Code Removed: put back if you want to limit the number of lines output
	int32_t	num_lines = SendMessage(native.out_window, EM_GETLINECOUNT, (WPARAM)NULL, (LPARAM)NULL);

	if ( num_lines > 1500 )
	{
		/* if we reach this stage, we're sure to be using up a fair amount of
		 * memory with all old text, so wipe it out */
		int32_t	num_char_in_line = SendMessage(native.out_window, EM_LINELENGTH, (WPARAM)0, (LPARAM)NULL);
		SendMessage(native.out_window, EM_SETSEL, (WPARAM)0, (LPARAM)num_char_in_line+1);   // +1 for skipped carriage return
		SendMessage(native.out_window, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)NULL);
	}
#endif

	SendMessage(native.out_window, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
	SendMessage(native.out_window, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION|SCF_WORD, (LPARAM)&_cf);
	SendMessage(native.out_window, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)output_text);

	SendMessage(native.out_window, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)NULL);

	/* we'll also output it to the log file, since some people are so
	 * useless they'll say 'it errored' without providing a screenshot or
	 * description of the error - so you have to run it all over again.. */
	if ( runtime.Config()->general.log )
	{
#if 0
		// do a raw append, otherwise we get date/time/level prefix
		//log->Append(output_mb);
#else
		/* newline needed, as each LOG() outputs the prefix info.. this
		 * in turn means we need to wipe out any others suffixing the
		 * buffer too */
		LOG(LL_Info) << str_trim(output_mb) << "\n";
#endif
	}
}



void
UserInterface::Prepare()
{
	HDC		hDC = GetDC(native.out_window);

	memset(&_cf, 0, sizeof(_cf));

	_cf.cbSize	= sizeof(CHARFORMAT2);
	_cf.dwMask	= CFM_ALL|CFM_ALL2|CFM_COLOR;
	_cf.yHeight	= -MulDiv(8*22, GetDeviceCaps(hDC, LOGPIXELSY), 72);     // gimme some magic numbers :p

	ReleaseDC(native.out_window, hDC);

	// fixed-width that's 'guaranteed' to be on Windows XP and above
	MbToUTF8(_cf.szFaceName, "Courier New", _countof(_cf.szFaceName));
	// background is all black, like the existing window
	_cf.crBackColor  = OUTCOLOR_BACKGROUND;
}



void
UserInterface::Update(
	Operation* operation
)
{
	if ( operation->HasExecuted() )
	{
		if ( operation->GetSuccess() )
		{
			if ( operation->WasNeeded() )
			{
				Output(OUTCOLOR_SUCCESS, "not needed\n");
			}
			else
			{
				Output(OUTCOLOR_SUCCESS, "ok!\n");
			}
		}
		else
		{
			Output(OUTCOLOR_ERROR, "failed\n");
			Output(OUTCOLOR_ERROR, "\tError Code %d (%s)\n", operation->GetErrorCode(), operation->GetErrorMsg().c_str());
		}
	}
	else
	{
		/* for recursive functions, additional operations will be added to
		 * the operations stack, and unfinished ones will not have output
		 * their success/failure, or a newline.
		 * When we resume it, the notify will enter here again automatically
		 * and output the operation details again - which is desired! */
		if ( operation->IsPaused() )
		{
			Output(OUTCOLOR_WARNING, "delayed\n");
			return;
		}

		switch ( operation->GetOperation() )
		{
		case OP_DeleteFile:
			{
				Output(OUTCOLOR_NORMAL, "Deleting File:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_DeleteDirectory:
			{
				Output(OUTCOLOR_NORMAL, "Removing Folder:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_KillProcess:
			{
				Output(OUTCOLOR_NORMAL, "Killing Process:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_KillThread:
			{
				Output(OUTCOLOR_NORMAL, "Killing Thread:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_ChangeOwnerFile:
			{
				Output(OUTCOLOR_NORMAL, "Taking Ownership:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_ChangeOwnerKey:
			{
				HKEY	hkey = (HKEY)operation->GetExtraData()->vparam1;
				if ( hkey == HKEY_LOCAL_MACHINE )
					strlcpy(mb, "HKLM", sizeof(mb));
				else if ( hkey == HKEY_USERS )
					strlcpy(mb, "HKU", sizeof(mb));
				else if ( hkey == HKEY_CURRENT_USER )
					strlcpy(mb, "HKCU", sizeof(mb));

				Output(OUTCOLOR_NORMAL, "Taking Ownership:\t'%s\\%s'... ", mb, operation->GetData().c_str());
				break;
			}
		case OP_ChangePermissionsFile:
			{
				Output(OUTCOLOR_NORMAL, "Replacing ACLs:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_ChangePermissionsKey:
			{
				HKEY	hkey = (HKEY)operation->GetExtraData()->vparam1;
				if ( hkey == HKEY_LOCAL_MACHINE )
					strlcpy(mb, "HKLM", sizeof(mb));
				else if ( hkey == HKEY_USERS )
					strlcpy(mb, "HKU", sizeof(mb));
				else if ( hkey == HKEY_CURRENT_USER )
					strlcpy(mb, "HKCU", sizeof(mb));

				Output(OUTCOLOR_NORMAL, "Replacing ACLs:\t'%s\\%s'... ", mb, operation->GetData().c_str());
				break;
			}
		case OP_DeleteRegKey:
			{
				HKEY	hkey = (HKEY)operation->GetExtraData()->vparam1;
				if ( hkey == HKEY_LOCAL_MACHINE )
					strlcpy(mb, "HKLM", sizeof(mb));
				else if ( hkey == HKEY_USERS )
					strlcpy(mb, "HKU", sizeof(mb));
				else if ( hkey == HKEY_CURRENT_USER )
					strlcpy(mb, "HKCU", sizeof(mb));

				Output(OUTCOLOR_NORMAL, "Deleting RegKey:\t'%s\\%s'... ", mb, operation->GetData().c_str());
				break;
			}
		case OP_DeleteRegValue:
			{
				HKEY	hkey = (HKEY)operation->GetExtraData()->vparam1;
				if ( hkey == HKEY_LOCAL_MACHINE )
					strlcpy(mb, "HKLM", sizeof(mb));
				else if ( hkey == HKEY_USERS )
					strlcpy(mb, "HKU", sizeof(mb));
				else if ( hkey == HKEY_CURRENT_USER )
					strlcpy(mb, "HKCU", sizeof(mb));

				Output(OUTCOLOR_NORMAL, 
					"Deleting RegValue:\t'%s\\%s' -> '%s'... ", 
					mb, operation->GetData().c_str(), operation->GetExtraData()->sparam1.c_str()
				);
				break;
			}
		case OP_DeleteService:
			{
				Output(OUTCOLOR_NORMAL, "Deleting Service:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_StopService:
			{
				Output(OUTCOLOR_NORMAL, "Stopping Service:\t'%s'... ", operation->GetData().c_str());
				break;
			}
		case OP_Execute:
			{
				Output(OUTCOLOR_NORMAL, "Executing:\t\t'%s'... ", operation->GetData().c_str());
				break;
			}
		// used for outputting text without any other formatting (like waiting for process messages)
		case OP_Dummy:
			{
				Output(OUTCOLOR_NORMAL, "%s... ", operation->GetData().c_str());
				break;
			}
		default:
			throw std::runtime_error("Unhandled Operation");
		}
	}
}
