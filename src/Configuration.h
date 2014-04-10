#ifndef CONFIGURATION_H_INCLUDED
#define CONFIGURATION_H_INCLUDED

/**
 * @file	Configuration.h
 * @author	James Warren
 * @brief	The application configuration
 */


// required inclusions
#include "build.h"
#include <string>
#include <map>


// forward declarations
enum E_FILE_TYPE;

// default definitions
#define DEFAULT_GRAPHICAL_UI			true
#define DEFAULT_PURGE_USE_UNDOCUMENTED		true
#define DEFAULT_FILEPURGE_CHANGE_PERMISSIONS	false
#define DEFAULT_FILEPURGE_TAKE_OWNERSHIP	false
#define DEFAULT_REGPURGE_CHANGE_PERMISSIONS	false
#define DEFAULT_REGPURGE_TAKE_OWNERSHIP		false
#define DEFAULT_PROCPURGE_ADVANCED		false
#define DEFAULT_LOGFILE				true

// command line options
#define CHAR_CMD_HELP				'h'
#define CHAR_CMD_CLI				'g'
#define CHAR_CMD_CONFIG_FILE			'c'
#define CHAR_CMD_SKIP_PROCESSES			'p'
#define CHAR_CMD_SKIP_FILES			'f'
#define CHAR_CMD_SKIP_REGISTRY			'r'
#define CHAR_CMD_SKIP_SERVICES			's'
#define CHAR_CMD_SKIP_COMMANDS			'e'
#define CHAR_CMD_SKIP_UNINSTALLERS		'u'
#define CHAR_CMD_REPLACE_PERMS			'k'
#define CHAR_CMD_TAKE_OWNERSHIP			'o'
#define CHAR_CMD_LOGFILE			'l'
#define CHAR_CMD_MULTIMETHOD			'm'
#define CHAR_CMD_UNDOCUMENTED			'x'



/**
 * Holds all of the configuration variables for the application, and provides
 * the ability to load from file, or retain the defaults.
 *
 * @class Configuration
 */
class Configuration
{
	// only the runtime is allowed to construct us
	friend class Runtime;
private:
	NO_CLASS_ASSIGNMENT(Configuration);
	NO_CLASS_COPY(Configuration);


	/** Flag for whether the config has been loaded */
	bool		_is_loaded;

	/** Path, absolute, to the config file */
	wchar_t		_file_path[MAX_LEN_GENERIC];


	// one controlled instance of this class; created in Runtime
	Configuration() : _is_loaded(false)
	{
		/* initialize all the configuration structs here; we cannot do
		 * this in the proxy<> constructor as templates like std::string 
		 * will fail to initialize! */
		general.log = true;
	}

public:
	~Configuration() {}


	/**
	 * Dumps the entire configuration, as-is, to the application log file.
	 * LL_Debug does not need to be set, as it will be output directly, not
	 * checking the level.
	 */
	void
	Dump() const;


	/**
	 * 
	 */
	void
	Load(
		const char* override_path
	);


	/**
	 * Retreives if the configuration has been loaded
	 *
	 * @return true if Load() has executed successfully
	 * @return false if Load() has not executed, or failed
	 */
	bool
	Loaded() const
	{ return _is_loaded; }


	/**
	 * Loads the default configuration options. Should be called first,
	 * before Load().
	 *
	 * The settings set here must constrast with those processed in the
	 * parse_commandline() function, as they are designed to override 
	 * anything set here.
	 */
	void
	LoadDefaults();


	/*
	 * Thank you guys @ StackOverflow:
	 * http://stackoverflow.com/questions/5424042/class-variables-public-access-read-only-but-private-access-read-write
	 */
	template <class T>
	class proxy
	{
		// Config class can always freely modify
		friend class Configuration;
		// Command-line parsing overrides file input/defaults
		friend bool parse_commandline(int32_t argc, char** argv);
	private:
		T data;
		T operator= (const T& arg) { data = arg; return data; }
	public:
		// 0/NUL/nullptr the datatype on construction
		proxy()
		{
		}
		operator const T&() const { return data; }
		operator const T&() { return data; }
	};

	// <option,interepreted_data>
	typedef std::map<char,std::string>	opts_map;


	struct {
		proxy<bool>		graphical;
	} ui;

	struct {
		proxy<bool>		log;
		proxy<std::string>	path;
		proxy<opts_map>		cmdline_opts;
	} general;

	struct {
		proxy<bool>		use_undocumented;

		struct {
			proxy<std::string>	path;
			proxy<bool>		change_permissions;
			proxy<bool>		take_ownership;
			proxy<bool>		no_proc;
		} file;
		struct {
			proxy<std::string>	path;
			proxy<bool>		change_permissions;
			proxy<bool>		take_ownership;
			proxy<bool>		no_proc;
		} registry;
		struct {
			proxy<std::string>	path;
			proxy<bool>		no_proc;
		} service;
		struct {
			proxy<std::string>	path;
			proxy<bool>		advanced_methods;
			proxy<bool>		no_proc;
		} process;
		struct {
			proxy<std::string>	path;
			proxy<bool>		no_proc;
		} command;
		struct {
			proxy<std::string>	path;
			proxy<bool>		no_proc;
		} uninstaller;
	} purging;
	

	// accessor functions


	/**
	 * The returned pointer is valid only as long as the configuration
	 * class exists - which, as it resides in the runtime, means anything
	 * within main can use this safely.
	 */
	const char*
	ConfigPath() const;


	/**
	 * The returned pointer is valid only as long as the configuration
	 * class exists - which, as it resides in the runtime, means anything
	 * within main can use this safely.
	 *
	 * @param[in] ft The file type to return the path for
	 */
	const char*
	PurgingPath(
		E_FILE_TYPE ft
	) const;
};



/**
 * Displays the command line usage. Triggered only when an invalid option, or
 * '-h', is passed in via the command line.
 *
 * @param[in] app_name The binary name executed
 * @param[in] opt The option character that was invalid
 */
void
display_usage(
	char* app_name,
	int32_t opt
);


/**
 * Parses the application command line, setting runtime configuration options.
 * Separate function from the Configuration as it should only be executed once,
 * and within app_init(), where the parameters are available.
 *
 * @param argc The number of arguments passed in
 * @param argv An array of pointers to the arguments
 * @return true is returned if any and all command line options are processed
 * successfully
 * @return false is only returned if an unknown parameter is provided, or an
 * invalid option is passed to a parameter
 */
bool
parse_commandline(
	int32_t argc,
	char** argv
);



#endif	// CONFIGURATION_H_INCLUDED
