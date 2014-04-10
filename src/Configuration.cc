
/**
 * @file	Configuration.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <iostream>		// cout
#include <Windows.h>		// ini files

#include "getopt.h"		// command line parsing
#include "Configuration.h"	// prototypes
#include "FileParser.h"		// E_FILE_TYPE
#include "utils.h"
#include "Runtime.h"
#include "Log.h"


#pragma warning ( disable : 4800 )	// forcing int to bool (is intentional here)


void
Configuration::Dump() const
{
	Log*	log = runtime.Logger();

	// we want to start on a newline, logging will have the prefix data
	LOG(LL_Force) << "\n\t==== Dumping Parsed Configuration ====\n";

	std::stringstream	log_str;

	log_str	<< "\t---- General Settings ----\n"
		<< "\t* config_path = " << general.path.data << "\n"
		<< "\t* logfile = " << general.log << "\n"
		<< "\t* use_undocumented_functions = " << purging.use_undocumented << "\n"
		<< "\t* graphical_ui = " << ui.graphical << "\n";

	log_str	<< "\t---- File/Folder Settings ----\n"
		<< "\t* path = " << purging.file.path.data << "\n"
		<< "\t* change_permissions = " << purging.file.change_permissions << "\n"
		<< "\t* take_ownership = " << purging.file.take_ownership << "\n"
		<< "\t* no_proc = " << purging.file.no_proc << "\n";

	log_str	<< "\t---- Registry Settings ----\n"
		<< "\t* path = " << purging.registry.path.data << "\n"
		<< "\t* change_permissions = " << purging.registry.change_permissions << "\n"
		<< "\t* take_ownership = " << purging.registry.take_ownership << "\n"
		<< "\t* no_proc = " << purging.registry.no_proc << "\n";

	log_str	<< "\t---- Process Settings ----\n"
		<< "\t* path = " << purging.process.path.data << "\n"
		<< "\t* multimethod_kill = " << purging.process.advanced_methods << "\n"
		<< "\t* no_proc = " << purging.process.no_proc << "\n";

	log_str	<< "\t---- Service Settings ----\n"
		<< "\t* path = " << purging.service.path.data << "\n"
		<< "\t* no_proc = " << purging.service.no_proc << "\n";

	log_str	<< "\t---- Uninstaller Settings ----\n"
		<< "\t* path = " << purging.uninstaller.path.data << "\n"
		<< "\t* no_proc = " << purging.uninstaller.no_proc << "\n";

	log_str	<< "\t---- Command Settings ----\n"
		<< "\t* path = " << purging.command.path.data << "\n"
		<< "\t* no_proc = " << purging.command.no_proc << "\n";

	log_str << "\t#### End Settings Dump ####\n";

	log->Append(log_str.str());
}



void
Configuration::Load(
	const char* override_path
)
{
	char		mb[MAX_PATH];
	wchar_t		ret[MAX_PATH];
	int32_t		val;
	// save mass code-spammage and illegibility..
	opts_map*		cfgmap = &general.cmdline_opts.data;
	opts_map::iterator	iter;

	/* var is currently brought in via a std::string, so is never a nullptr;
	 * we leave the check here though in case of future amendment */
	if ( override_path == nullptr || strlen(override_path) == 0 )
	{
		// configuration file is at the current bin path, called 'config.ini'
		get_current_binary_path(mb, sizeof(mb));
		strlcat(mb, "config.ini", sizeof(mb));
	}
	else
	{
		char*	p;
		
		/* if a config file is specified different from that of the
		 * current directory, switch to it; we won't be able to find the
		 * files unless the path is given in the config file itself,
		 * which puts a burden on the user and is error-prone. */

		if (( p = (char*)strchr(override_path, ':')) != nullptr )
		{
			// drive letter specified
			strlcpy(mb, override_path, sizeof(mb));
		}
		else if ( strncmp(override_path, "\\\\", 2) == 0 )
		{
			// network path specified
			strlcpy(mb, override_path, sizeof(mb));
		}
		else
		{
			// current directory offset assumed
			get_current_binary_path(mb, sizeof(mb));
			strlcat(mb, override_path, sizeof(mb));
		}
		
		if (( p = strrchr(mb, '\\')) != nullptr )
		{
			// check msdn SetCurrentDirectory for this reasoning...
			if ( (strlen(mb) - strlen(p)) > (MAX_PATH - 2) )
			{
				throw std::runtime_error("Path length exceeds maximum allowed");
			}

			// nul the separator so we can switch, then restore it
			*p = '\0';
			if ( !SetCurrentDirectoryA(mb) )
			{
				DWORD	last_error = GetLastError();
				std::string	err = "SetCurrentDirectory() failed; ";
				err += error_code_as_string(last_error);
				throw std::runtime_error(err);
			}
			*p = '\\';
		}
	}

	MbToUTF8(_file_path, mb, _countof(_file_path));
	

	// check the file actually exists before we march on...
	if ( GetFileAttributes(_file_path) == INVALID_FILE_ATTRIBUTES )
	{
		DWORD		err = GetLastError();
		std::string	msg = "Could not open '";
		
		msg += mb;
		msg += "'; error: ";
		msg += error_code_as_string(err);

		throw std::runtime_error(msg);
	}


	/* now we can search the map containing all the command-line options,
	 * and skip reading from the file if they've been set. */

	if (( iter = cfgmap->find(CHAR_CMD_LOGFILE)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"winwipe", L"logfile", -1, _file_path)) != -1 )
			general.log = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_CLI)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"winwipe", L"graphical_ui", -1, _file_path)) != -1 )
			ui.graphical = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_UNDOCUMENTED)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"winwipe", L"use_undocumented_functions", -1, _file_path)) != -1 )
			purging.use_undocumented = val;
	}

	if (( iter = cfgmap->find(CHAR_CMD_SKIP_FILES)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"files", L"no_proc", -1, _file_path)) != -1 )
			purging.file.no_proc = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_SKIP_REGISTRY)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"registry", L"no_proc", -1, _file_path)) != -1 )
			purging.registry.no_proc = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_SKIP_PROCESSES)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"processes", L"no_proc", -1, _file_path)) != -1 )
			purging.process.no_proc = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_SKIP_SERVICES)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"services", L"no_proc", -1, _file_path)) != -1 )
			purging.service.no_proc = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_SKIP_COMMANDS)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"commands", L"no_proc", -1, _file_path)) != -1 )
			purging.command.no_proc = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_SKIP_UNINSTALLERS)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"uninstallers", L"no_proc", -1, _file_path)) != -1 )
			purging.uninstaller.no_proc = val;
	}

	if (( iter = cfgmap->find(CHAR_CMD_TAKE_OWNERSHIP)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"files", L"take_ownership", -1, _file_path)) != -1 )
			purging.file.take_ownership = val;
		if (( val = GetPrivateProfileInt(L"registry", L"take_ownership", -1, _file_path)) != -1 )
			purging.registry.take_ownership = val;
	}
	if (( iter = cfgmap->find(CHAR_CMD_REPLACE_PERMS)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"files", L"change_permissions", -1, _file_path)) != -1 )
			purging.file.change_permissions = val;
		if (( val = GetPrivateProfileInt(L"registry", L"change_permissions", -1, _file_path)) != -1 )
			purging.registry.change_permissions = val;
	}
	
	if (( iter = cfgmap->find(CHAR_CMD_MULTIMETHOD)) == cfgmap->end() )
	{
		if (( val = GetPrivateProfileInt(L"processes", L"multimethod_kill", -1, _file_path)) != -1 )
			purging.process.advanced_methods = val;
	}

	//if (( val = GetPrivateProfileInt(L"services", L"stop_before_delete", -1, _file_path)) != -1 )
	//	purging.service.stop_before_delete = val;


	// our multi-byte/unicode style causes complications
	if ( GetPrivateProfileString(L"files", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.file.path = mb;
	}
	if ( GetPrivateProfileString(L"registry", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.registry.path = mb;
	}
	if ( GetPrivateProfileString(L"processes", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.process.path = mb;
	}
	if ( GetPrivateProfileString(L"services", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.service.path = mb;
	}
	if ( GetPrivateProfileString(L"commands", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.command.path = mb;
	}
	if ( GetPrivateProfileString(L"uninstallers", L"path", L"", ret, _countof(ret), _file_path) > 0 )
	{
		UTF8ToMb(mb, ret, sizeof(mb));
		purging.uninstaller.path = mb;
	}

	_is_loaded = true;
}



void
Configuration::LoadDefaults()
{
	general.log				= DEFAULT_LOGFILE;
	purging.file.no_proc			= false;
	purging.file.change_permissions		= DEFAULT_FILEPURGE_CHANGE_PERMISSIONS;
	purging.file.take_ownership		= DEFAULT_FILEPURGE_TAKE_OWNERSHIP;
	purging.registry.no_proc		= false;
	purging.registry.change_permissions	= DEFAULT_REGPURGE_CHANGE_PERMISSIONS;
	purging.registry.take_ownership		= DEFAULT_REGPURGE_TAKE_OWNERSHIP;
	purging.process.no_proc			= false;
	purging.process.advanced_methods	= DEFAULT_PROCPURGE_ADVANCED;
	purging.command.no_proc			= false;
	purging.uninstaller.no_proc		= false;
	purging.use_undocumented		= DEFAULT_PURGE_USE_UNDOCUMENTED;
	ui.graphical				= DEFAULT_GRAPHICAL_UI;
}



const char*
Configuration::ConfigPath() const
{
	return general.path.data.c_str();
}



const char*
Configuration::PurgingPath(
	E_FILE_TYPE ft
) const
{
	switch ( ft )
	{
	case FT_Files:		return purging.file.path.data.c_str();
	case FT_Processes:	return purging.process.path.data.c_str();
	case FT_Registry:	return purging.registry.path.data.c_str();
	case FT_Services:	return purging.service.path.data.c_str();
	case FT_Commands:	return purging.command.path.data.c_str();
	case FT_Uninstallers:	return purging.uninstaller.path.data.c_str();
	default:
		return nullptr;
	}
}



void
display_usage(
	char* app_name,
	int32_t opt
)
{
	if ( opt != -1 )
		printf("\nUnrecognized Option: %c\n\n", opt);

	printf("Usage:\t%s [options]\n", app_name);
	printf(
		"\nCommand-line options override a configuration file where set:\n\n"
		"-%c\tDisplays this help\n"
		"-%c\tUse CLI mode (currently non-functional; do not use)\n"
		"-%c <path>\tConfiguration file to use (default: config.ini)\n"
		"-%c\tSkip processes\n"
		"-%c\tSkip files and folders\n"
		"-%c\tSkip registry\n"
		"-%c\tSkip services\n"
		"-%c\tSkip commands\n"
		"-%c\tSkip uninstallers\n"
		"-%c\tReplace permissions (when needed)\n"
		"-%c\tTake ownership (when needed)\n"
		"-%c\tMulti-method process kill (may be unstable)\n"
		"-%c\tUse undocumented API functions (stable only on known Windows versions)\n"
		"-%c\tDon't create a logfile\n"
		"\n",
		CHAR_CMD_HELP,
		CHAR_CMD_CLI,
		CHAR_CMD_CONFIG_FILE,
		CHAR_CMD_SKIP_PROCESSES,
		CHAR_CMD_SKIP_FILES,
		CHAR_CMD_SKIP_REGISTRY,
		CHAR_CMD_SKIP_SERVICES,
		CHAR_CMD_SKIP_COMMANDS,
		CHAR_CMD_SKIP_UNINSTALLERS,
		CHAR_CMD_REPLACE_PERMS,
		CHAR_CMD_TAKE_OWNERSHIP,
		CHAR_CMD_MULTIMETHOD,
		CHAR_CMD_UNDOCUMENTED,
		CHAR_CMD_LOGFILE
	);
}



bool
parse_commandline(
	int32_t argc,
	char** argv
)
{
	Configuration*	cfg = runtime.Config();
	char		getopt_str[32];
	int32_t		opt;

	str_format(getopt_str, sizeof(getopt_str),
		"%c:%c%c%c%c%c%c%c%c%c%c%c%c%c",
		CHAR_CMD_CONFIG_FILE,
		CHAR_CMD_CLI,
		CHAR_CMD_HELP,
		CHAR_CMD_MULTIMETHOD,
		CHAR_CMD_REPLACE_PERMS,
		CHAR_CMD_SKIP_COMMANDS,
		CHAR_CMD_SKIP_FILES,
		CHAR_CMD_SKIP_PROCESSES,
		CHAR_CMD_SKIP_REGISTRY,
		CHAR_CMD_SKIP_SERVICES,
		CHAR_CMD_SKIP_UNINSTALLERS,
		CHAR_CMD_TAKE_OWNERSHIP,
		CHAR_CMD_UNDOCUMENTED,
		CHAR_CMD_LOGFILE
	);

	// VS2008 dislikes 'int32_t to const char' in the std::pairs
#if MSVC_BEFORE_VS10
#	pragma warning ( push )
#	pragma warning ( disable : 4244 )
#endif

	opt = getopt(argc, argv, getopt_str);

	while ( opt != -1 )
	{
		switch ( opt )
		{
		case CHAR_CMD_LOGFILE:
			// don't create a logfile
			cfg->general.log = false;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "0")
			);
			break;
		case CHAR_CMD_CLI:
			// stay in cli mode (useful for running remotely)
			cfg->ui.graphical = false;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "0")
			);
			break;
		case CHAR_CMD_SKIP_PROCESSES:
			// skip processes
			cfg->purging.process.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_SKIP_FILES:
			// skip files/folders
			cfg->purging.file.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_SKIP_REGISTRY:
			// skip registry
			cfg->purging.registry.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_SKIP_SERVICES:
			// skip services
			cfg->purging.service.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_SKIP_UNINSTALLERS:
			// skip uninstallers
			cfg->purging.uninstaller.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_SKIP_COMMANDS:
			// skip commands
			cfg->purging.command.no_proc = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_REPLACE_PERMS:
			// replace permissions where needed
			cfg->purging.file.change_permissions = true;
			cfg->purging.registry.change_permissions = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_TAKE_OWNERSHIP:
			// replace ownership where needed
			cfg->purging.file.take_ownership = true;
			cfg->purging.registry.take_ownership = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_MULTIMETHOD:
			// use all capabilities to terminate processes
			cfg->purging.process.advanced_methods = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_CONFIG_FILE:
			// specify configuration file instead of default
			cfg->general.path = getopt_arg;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, getopt_arg)
			);
			break;
		case CHAR_CMD_UNDOCUMENTED:
			// use undocumented functions
			cfg->purging.use_undocumented = true;
			cfg->general.cmdline_opts.data.insert(
				std::pair<char,std::string>(opt, "1")
			);
			break;
		case CHAR_CMD_HELP:
			opt = -1;
			// fall through
		default:
			display_usage(argv[0], opt);
			return false;
		}

		opt = getopt(argc, argv, getopt_str);
	}

#if MSVC_BEFORE_VS10
#	pragma warning ( pop )
#endif

	// nothing invalid or help, so startup can proceed
	return true;
}
