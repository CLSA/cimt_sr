/*=========================================================================

  Program:  sampsim
  Module:   options.h
  Language: C++

=========================================================================*/

#include "options.h"

//#include "utilities.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

//namespace sampsim
//{
  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  std::string options::option::get_value( const unsigned int index ) const
  {
    if( index >= this->initial_values.size() )
    {
      std::stringstream stream;
      stream << "ERROR: Option ";
      if( 0 < this->long_name.length() ) stream << "\"" << this->long_name << "\"";
      else if( ' ' != short_name ) stream << "\"" << this->short_name << "\"";
      stream << " index " << index << " is out of range";
      throw std::runtime_error( stream.str() );
    }

    return index < this->values.size() ? this->values[index] : this->initial_values[index];
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  options::options( const std::string executable_name )
  {
    this->usage_printed = false;
    this->executable_name = executable_name;
    this->add_flag( 'h', "help", "Prints usage help" );
    this->add_option( 'c', "config", "", "A configuration file containing flags and options" );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::add_heading( const std::string description )
  {
    this->usage_list.push_back( description );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::add_input( const std::string name )
  {
    this->input_map[name] = "";
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::add_flag(
    const char short_name,
    const std::string long_name,
    const std::string description )
  {
    flag flag;
    flag.short_name = short_name;
    flag.long_name = long_name;
    flag.description = description;
    this->flag_list.push_back( flag );

    // now add the usage string for this flag
    std::stringstream stream;
    stream << " ";
    if( flag.has_short_name() ) stream << "-" << flag.short_name << "  ";
    if( flag.has_long_name() ) stream << "--" << flag.long_name;
    std::string spacing;
    int spacing_length = 30 - stream.str().length();
    if( 2 > spacing_length ) spacing_length = 2;
    spacing.assign( spacing_length, ' ' );
    stream << spacing << flag.description;
    this->add_heading( stream.str() );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::add_option(
    const char short_name,
    const std::string long_name,
    const std::vector< std::string > initial_values,
    const std::string description )
  {
    option option;
    option.short_name = short_name;
    option.long_name = long_name;
    option.initial_values = initial_values;
    option.description = description;
    this->option_list.push_back( option );

    // now add the usage string for this option
    std::stringstream stream;
    stream << " ";
    if( option.has_short_name() ) stream << "-" << option.short_name << "  ";
    if( option.has_long_name() ) stream << "--" << option.long_name;
    std::string spacing;
    int spacing_length = 30 - stream.str().length();
    if( 2 > spacing_length ) spacing_length = 2;
    spacing.assign( spacing_length, ' ' );
    stream << spacing << option.description;
    this->add_heading( stream.str() );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::set_arguments( const int argc, const char** argv )
  {
    this->argument_list.clear();
    for( int c = 0; c < argc; c++ ) this->argument_list.push_back( argv[c] );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  bool options::process()
  {
    bool config_file_processed = false;
    bool success = this->process_arguments();

    if( success )
    {
      std::string config_file = this->get_option( "config" );
      while( 0 < config_file.length() )
      {
        this->find_option( "config" )->values.clear();
        if( !this->process_config_file( config_file ) )
        {
          config_file_processed = false;
          success = false;
          break;
        }
        config_file = this->get_option( "config" );
        if( false == config_file_processed ) config_file_processed = true;
      }

      // command line arguments override config file arguments
      if( config_file_processed ) success = this->process_arguments();
    }

    return success;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  bool options::process_arguments()
  {
    bool invalid = false, out_of_range = false, missing_value = false;
    char short_name = ' ';
    std::string long_name = "";
    std::string error = "";
    flag *flag;
    option *option;

    // make a temporary vector of all inputs
    std::vector< std::string > input_list;

    for( auto it = this->argument_list.cbegin(); it != this->argument_list.cend(); ++it )
    {
      // skip the first argument (it's the executable's name
      if( this->argument_list.cbegin() == it ) continue;

      std::string arg = (*it);
      if( "--" == arg.substr( 0, 2 ) )
      { // defining a long name
        if( 3 >= arg.length() ) invalid = true; // options must be more than one character long
        else if( ' ' != short_name ) missing_value = true; // already defining an option
        else if( 0 < long_name.length() ) missing_value = true; // already defining an option
        else
        {
          long_name = arg.substr( 2, std::string::npos );

          if( NULL != ( flag = this->find_flag( long_name ) ) )
          { // defining a flag
            flag->value = true;
            long_name = "";
          }
          else if( NULL == ( option = this->find_option( long_name ) ) )
          { // not defining an option either
            invalid = true;
          }
        }
      }
      else if( '-' == arg[0] )
      { // defning a short name
        if( 2 != arg.length() ) invalid = true; // short options must be one character only
        else if( ' ' != short_name ) missing_value = true; // already defining an option
        else if( 0 < long_name.length() ) missing_value = true; // already defining an option
        else
        {
          short_name = arg[1];

          // see if this is a flag
          if( NULL != ( flag = this->find_flag( short_name ) ) )
          { // defining a flag
            flag->value = true;
            short_name = ' ';
          }
          else if( NULL == ( option = this->find_option( short_name ) ) )
          { // not defining an option either
            invalid = true;
          }
        }
      }
      else
      { // defining a value
        if( 0 < long_name.length() )
        { // expecting a value for an option identified by its long name
          std::vector< std::string > values = options::explode( arg, "," );
          if( values.size() <= option->initial_values.size() )
          {
            option->values = values;
            long_name = "";
          }
          else out_of_range = true;
        }
        else if( ' ' != short_name )
        { // expecting a value for an option identified by its short name
          std::vector< std::string > values = options::explode( arg, "," );
          if( values.size() <= option->initial_values.size() )
          {
            option->values = values;
            short_name = ' ';
          }
          else out_of_range = true;
        }
        else
        {
          input_list.push_back( arg );
        }
      }

      if( invalid )
      {
        std::cout << "ERROR: Invalid ";
        if( 0 < long_name.length() ) std::cout << "option \"--" << long_name << "\"";
        else if( ' ' != short_name ) std::cout << "option \"-" << short_name << "\"";
        else std::cout << "argument \"" << arg << "\"";
        std::cout << std::endl;
        break;
      }
      else if( out_of_range )
      {
        std::cout << "ERROR: Number of values for option ";
        if( 0 < long_name.length() ) std::cout << "\"" << long_name << "\"";
        else if( ' ' != short_name ) std::cout << "\"" << short_name << "\"";
        std::cout << " is out of range (expecting up to " << option->initial_values.size() << " value)"
                  << std::endl;
        break;
      }
      else if( missing_value )
      {
        std::cout << "ERROR: Expecting value for option ";
        if( 0 < long_name.length() ) std::cout << "\"" << long_name << "\"";
        else if( ' ' != short_name ) std::cout << "\"" << short_name << "\"";
        std::cout << " but none provided" << std::endl;
        break;
      }
    }

    // make sure there are no left over options
    if( !( invalid || out_of_range || missing_value ) )
    {
      if( 0 < long_name.length() )
      {
        std::cout << "ERROR: expecting a value after \"--" << long_name << "\"" << std::endl;
        invalid = true;
      }
      else if( ' ' != short_name )
      {
        std::cout << "ERROR: expecting a value after \"-" << short_name << "\"" << std::endl;
        invalid = true;
      }

      // only process the input arguments if the help switch is off
      if( !this->find_flag( "help" )->value )
      {
        // now make sure we have the correct number of inputs
        if( input_list.size() != this->input_map.size() )
        {
          std::cout << "ERROR: Wrong number of free (input) arguments (expecting "
                    << this->input_map.size() << ", got " << input_list.size() << ")" << std::endl;
          invalid = true;
        }
        else
        {
          // populate the input map
          auto list_it = input_list.cbegin();
          auto map_it = this->input_map.rbegin();

          for( ; list_it != input_list.cend() && map_it != this->input_map.rend(); ++list_it, ++map_it )
            map_it->second = (*list_it);
        }
      }
    }

    if( invalid || out_of_range || missing_value )
      std::cout << "Try '" << this->executable_name << " --help' for more information." << std::endl;

    return !( invalid || out_of_range || missing_value );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  bool options::process_config_file( const std::string filename )
  {
    bool invalid = false;
    std::string line, key, value;
    flag* flag;
    option* option;

    std::ifstream file( filename, std::ifstream::in );
    if( !file.good() )
    {
      std::stringstream stream;
      stream << "Error opening file \"" << filename << "\"";
      throw std::runtime_error( stream.str() );
    }

    while( !file.eof() )
    {
      std::getline( file, line );

      // anything after a # is ignored, option and value is deliminated by :
      std::vector< std::string > parts = options::explode( line.substr( 0, line.find( '#' ) ), ":" );

      if( 1 == parts.size() )
      { // flag
        key = options::trim( parts[0] );

        flag = NULL;
        if( 1 == key.length() ) flag = this->find_flag( key[0] );
        else flag = this->find_flag( key );

        if( NULL != flag ) flag->value = true;
        else invalid = true;
      }
      else if( 2 == parts.size() )
      { // option
        key = options::trim( parts[0] );
        value = options::trim( parts[1] );

        option = NULL;
        if( 1 == key.length() ) option = this->find_option( key[0] );
        else option = this->find_option( key );

        if( NULL != option ) option->values = options::explode( value, "," );
        else invalid = true;
      }
      else if( 2 < parts.size() ) invalid = true;

      if( invalid )
      {
        std::cout << "ERROR: Error parsing config file at \"" << line << "\"" << std::endl;
        break;
      }
    }

    file.close();

    return !invalid;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  void options::print_usage()
  {
    if( !this->usage_printed )
    {
      std::cout << this->get_usage();
      this->usage_printed = true;
    }
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  std::string options::get_usage() const
  {
    // start by printing the command line
    std::stringstream stream;
    stream << "Usage: " << this->executable_name << " [options...]";
    for( auto input_it = this->input_map.crbegin(); input_it != this->input_map.crend(); ++input_it )
      stream << " <" << input_it->first << ">";
    stream << std::endl << std::endl;

    // then the options/flags/comments
    for( auto usage_it = this->usage_list.cbegin(); usage_it != this->usage_list.cend(); ++usage_it )
      stream << (*usage_it) << std::endl;
    return stream.str();
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  std::string options::get_input( const std::string name ) const
  {
    auto it = this->input_map.find( name );
    if( this->input_map.cend() == it )
    {
      std::stringstream stream;
      stream << "Tried to get input \"" << name << "\" which doesn't exist";
      throw std::runtime_error( stream.str() );
    }

    return it->second;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  bool options::get_flag( const char short_name, const std::string long_name ) const
  {
    for( auto it = this->flag_list.cbegin(); it != this->flag_list.cend(); ++it )
      if( ( 0 < long_name.length() && it->long_name == long_name ) ||
          ( ' ' != short_name && it->short_name == short_name ) ) return it->value;

    // if we got here then the flag isn't in the flag list
    std::stringstream stream;
    stream << "Tried to get value of invalid flag \"";
    if( 0 < long_name.length() ) stream << long_name;
    else stream << short_name;
    stream << "\"";
    throw std::runtime_error( stream.str() );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  std::vector< std::string > options::get_option_list(
    const char short_name, const std::string long_name ) const
  {
    std::vector< std::string > list;

    for( auto it = this->option_list.cbegin(); it != this->option_list.cend(); ++it )
      if( ( 0 < long_name.length() && it->long_name == long_name ) ||
          ( ' ' != short_name && it->short_name == short_name ) )
    {
      unsigned int size = it->initial_values.size();
      list.resize( size );
      for( unsigned int index = 0; index < size; index++ ) list[index] = it->get_value( index );
      break;
    }

    return list;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  std::string options::get_option(
    const char short_name, const std::string long_name, const unsigned int index ) const
  {
    for( auto it = this->option_list.cbegin(); it != this->option_list.cend(); ++it )
      if( ( 0 < long_name.length() && it->long_name == long_name ) ||
          ( ' ' != short_name && it->short_name == short_name ) ) return it->get_value( index );

    // if we got here then the option isn't in the option list
    std::stringstream stream;
    stream << "Tried to get value of invalid option \"";
    if( 0 < long_name.length() ) stream << long_name;
    else stream << short_name;
    stream << "\"";
    throw std::runtime_error( stream.str() );
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  options::flag* options::find_flag( const char short_name )
  {
    for( auto flag_it = this->flag_list.begin(); flag_it != this->flag_list.end(); ++flag_it )
      if( flag_it->has_short_name() && short_name == flag_it->short_name ) return &( *flag_it );
    return NULL;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  options::flag* options::find_flag( const std::string long_name )
  {
    for( auto flag_it = this->flag_list.begin(); flag_it != this->flag_list.end(); ++flag_it )
      if( flag_it->has_long_name() && long_name == flag_it->long_name ) return &( *flag_it );
    return NULL;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  options::option* options::find_option( const char short_name )
  {
    for( auto option_it = this->option_list.begin(); option_it != this->option_list.end(); ++option_it )
      if( option_it->has_short_name() && short_name == option_it->short_name ) return &( *option_it );
    return NULL;
  }

  //-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-+#+-
  options::option* options::find_option( const std::string long_name )
  {
    for( auto option_it = this->option_list.begin(); option_it != this->option_list.end(); ++option_it )
      if( option_it->has_long_name() && long_name == option_it->long_name ) return &( *option_it );
    return NULL;
  }
//}
