/*=========================================================================

  Program:  sampsim
  Module:   options.h
  Language: C++

=========================================================================*/

//#ifndef __sampsim_options_h
//#define __sampsim_options_h
#ifndef options_h
#define options_h

#if __cplusplus <= 199711L
  #error This library needs at least a C++11 compliant compiler
#endif

#include <algorithm>
#include <map>
#include <string>
#include <vector>

/**
 * @addtogroup sampsim
 * @{
 */

//namespace sampsim
//{
  /**
   * @class options
   * @author Patrick Emond <emondpd@mcmaster.ca>
   * @brief A class used to parse command line options
   * @details
   * This class provides a way to parse, process and manage command line options.  It works by first
   * defining all possible options and the number of input parameters expected from a command line
   * based command.  Additionally, configuration files can also be provided which may include values
   * for flags and options.  No more than one parameter may be included per line.  Options with no
   * values (flags) are set to true if found on a line alone.  For example:
   * 
   * verbose
   * 
   * Options with values must be separated by a ":" character.  For example:
   * 
   * seed: 1234
   * 
   * If an option includes only numbers, periods and commas (no other characters or spaces)
   * then it will be inturpreted as an array.  For instance:
   * 
   * coordinate: 1.2,2.4,3.6
   * 
   * Anything proceeding a # is ignored as a comment.
   */
  class options
  {
  private:
    /**
     * @struct flag
     * @brief An internal struct for handling flags
     */
    struct flag
    {
      /**
       * Constructor
       */
      flag() : short_name( ' ' ), long_name( "" ), value( false ), description( "" ) {};

      /**
       * The flag's short (single character) name
       */
      char short_name;

      /**
       * The flag's long name
       */
      std::string long_name;

      /**
       * Whether the flag has been raised or not
       */
      bool value;

      /**
       * The flag's description
       */
      std::string description;

      /**
       * Returns whether the flag has a short (single character) name or not
       */
      bool has_short_name() const { return ' ' != this->short_name; }

      /**
       * Returns whether the flag has a long name or not
       */
      bool has_long_name() const { return 0 < this->long_name.length(); }
    };

    /**
     * @struct flag
     * @brief An internal struct for handling options
     */
    struct option
    {
      /**
       * Constructor
       */
      option() :
        short_name( ' ' ),
        long_name( "" ),
        description( "" ) {};

      /**
       * The option's short (single character) name
       */
      char short_name;

      /**
       * The option's long name
       */
      std::string long_name;

      /**
       * The option's default array values
       */
      std::vector< std::string > initial_values;

      /**
       * The option's comma-deliniated set of values
       */
      std::vector< std::string > values;

      /**
       * The option's description
       */
      std::string description;

      /**
       * Returns whether the option has a short (single character) name or not
       */
      bool has_short_name() const { return ' ' != this->short_name; }

      /**
       * Returns whether the option has a long name or not
       */
      bool has_long_name() const { return 0 < this->long_name.length(); }

      /**
       * Returns the value of the option (returns the default value if no value has been set)
       * 
       * If the index is outside the range of values in the initial_values an exception is thrown.
       */
      std::string get_value( const unsigned int index = 0 ) const;
    };

  public:
    /**
     * Constructor
     */
    options( const std::string executable_name );

    /**
     * Adds a heading to the usage message
     * 
     * This method can be used to insert a line with any text.  An end-of-line character is added to
     * the end of the description string, so it should be called once for every line which is to be
     * inserted into the usage message.  Calling this method with an empty string will add an empty
     * line in the usage message.
     */
    void add_heading( const std::string description );

    /**
     * Adds an input to the expected command line arguments
     * 
     * Inputs are mandatory values without a preceeding switch.
     * For example: ./command file1 file2 (where file1 and file2 are inputs).
     */
    void add_input( const std::string name );

    /**
     * Adds an optional flag to the command line arguments
     * 
     * Flags are optional switches which, if included, sets the flag's value to true.
     * For example: ./command -a --super (where the flags 'a' and "super" are set to true).
     */
    void add_flag(
      const char short_name,
      const std::string long_name,
      const std::string description );

    /**
     * Convenience method for a flag without a long name
     */
    void add_flag( const char short_name, const std::string description )
    { this->add_flag( short_name, "", description ); }

    /**
     * Convenience method for a flag without a short name
     */
    void add_flag( const std::string long_name, const std::string description )
    { this->add_flag( ' ', long_name, description ); }

    /**
     * Adds an optional parameter to the command line arguments
     * 
     * Options are optional parameters which, if included, sets the parameter to a particular set of
     * values.  The maximum number of comma-deliniated values that an option may accept is defined by
     * the initial_values option.  If the option for a particular index is not provided then the initial
     * array option will be used.  If more options are provided than there are initial options then the
     * class will report an error.
     * For example: ./command --method simple (where the first and only element for the option "method"
     *                                         is set to "simple")
     *              ./command --multi a,3,test (where the first, second and third elements for the
     *                                          option "multi" is set to "a", "3" and "test"
     */
    void add_option(
      const char short_name,
      const std::string long_name,
      const std::vector< std::string > initial_values,
      const std::string description );

    /**
     * Convenience method for an option with a single element allowed
     */
    void add_option(
      const char short_name,
      const std::string long_name,
      const std::string initial_value,
      const std::string description )
    {
      std::vector< std::string > initial_values;
      initial_values.push_back( initial_value );
      this->add_option( short_name, long_name, initial_values, description );
    }

    /**
     * Convenience method for an option without a long name
     */
    void add_option(
      const char short_name,
      const std::vector< std::string > initial_values,
      const std::string description )
    { this->add_option( short_name, "", initial_values, description ); }

    /**
     * Convenience method for an option without a short name
     */
    void add_option(
      const std::string long_name,
      const std::vector< std::string > initial_values,
      const std::string description )
    { this->add_option( ' ', long_name, initial_values, description ); }

    /**
     * Convenience method for an option with a single element without a long name
     */
    void add_option(
      const char short_name,
      const std::string initial_value,
      const std::string description )
    { this->add_option( short_name, "", initial_value, description ); }

    /**
     * Convenience method for an option with a single element without a short name
     */
    void add_option(
      const std::string long_name,
      const std::string initial_value,
      const std::string description )
    { this->add_option( ' ', long_name, initial_value, description ); }

    /**
     * Sets the command line arguments
     * 
     * Provide command line arguments in the standard argc and argv forms.  These are the arguments passed
     * to the application's main() function.
     */
    void set_arguments( const int argc, const char** argv );

    /**
     * Process the command line arguments
     * 
     * Processes the arguments provided to the class by set_arguments(), returning true if all arguments
     * were parsed successfully.
     */
    bool process();

    /**
     * Prints the usage message
     */
    void print_usage();

    /**
     * Returns the parsed value for a particular input
     * 
     * Inputs are defined sequentially by the add_input() method.  This method returns the input provided
     * by the command line at the same position as the named input.
     */
    std::string get_input( const std::string name ) const;

    /**
     * Returns true if a flag has been raised (included in the command line)
     */
    bool get_flag( const std::string long_name ) const
    { return this->get_flag( ' ', long_name ); }

    /**
     * Returns true if a flag has been raised (included in the command line)
     */
    bool get_flag( const char short_name ) const
    { return this->get_flag( short_name, "" ); }

    /**
     * Returns the list of values of an option, and the default value where none is provided
     */
    std::vector< std::string > get_option_list( const char short_name ) const
    { return this->get_option_list( short_name, "" ); }

    /**
     * Returns the list of values of an option, and the default value where none is provided
     */
    std::vector< std::string > get_option_list( const std::string long_name ) const
    { return this->get_option_list( ' ', long_name ); }

    /**
     * Returns the value of an option, or the default value if none is provided
     */
    std::string get_option( const char short_name, const unsigned int index = 0 ) const
    { return this->get_option( short_name, "", index ); }

    /**
     * Returns the value of an option, or the default value if none is provided
     */
    std::string get_option( const std::string long_name, const unsigned int index = 0 ) const
    { return this->get_option( ' ', long_name, index ); }

    /**
     * Convenience method to return options as a list of integers
     */
    std::vector< int > get_option_as_int_list( const char short_name ) const
    {
      std::vector< std::string > option_list = this->get_option_list( short_name );
      std::vector< int > int_option_list;
      for( auto it = option_list.cbegin(); it != option_list.cend(); ++it )
        int_option_list.push_back( atoi( it->c_str() ) );
      return int_option_list;
    }

    /**
     * Convenience method to return options as a list of integers
     */
    std::vector< int > get_option_as_int_list( const std::string long_name ) const
    {
      std::vector< std::string > option_list = this->get_option_list( long_name );
      std::vector< int > int_option_list;
      for( auto it = option_list.cbegin(); it != option_list.cend(); ++it )
        int_option_list.push_back( atoi( it->c_str() ) );
      return int_option_list;
    }

    /**
     * Convenience method to return an option as an integer
     */
    int get_option_as_int( const char short_name, const unsigned int index = 0 ) const
    { return atoi( this->get_option( short_name, index ).c_str() ); }

    /**
     * Convenience method to return an option as an integer
     */
    int get_option_as_int( const std::string long_name, const unsigned int index = 0 ) const
    { return atoi( this->get_option( long_name, index ).c_str() ); }

    /**
     * Convenience method to return options as a list of doubles
     */
    std::vector< double > get_option_as_double_list( const char short_name ) const
    {
      std::vector< std::string > option_list = this->get_option_list( short_name );
      std::vector< double > double_option_list;
      for( auto it = option_list.cbegin(); it != option_list.cend(); ++it )
        double_option_list.push_back( atoi( it->c_str() ) );
      return double_option_list;
    }

    /**
     * Convenience method to return options as a list of doubles
     */
    std::vector< double > get_option_as_double_list( const std::string long_name ) const
    {
      std::vector< std::string > option_list = this->get_option_list( long_name );
      std::vector< double > double_option_list;
      for( auto it = option_list.cbegin(); it != option_list.cend(); ++it )
        double_option_list.push_back( atof( it->c_str() ) );
      return double_option_list;
    }

    /**
     * Convenience method to return an option as a double
     */
    double get_option_as_double( const char short_name, const unsigned int index = 0 ) const
    { return atof( this->get_option( short_name, index ).c_str() ); }

    /**
     * Convenience method to return an option as a double
     */
    double get_option_as_double( const std::string long_name, const unsigned int index = 0 ) const
    { return atof( this->get_option( long_name, index ).c_str() ); }

  private:
    /**
     * Generates the usage message and returns it as a string
     */
    std::string get_usage() const;

    /**
     * Internal method for returning the value of a flag
     */
    bool get_flag( const char short_name, const std::string long_name ) const;

    /**
     * Internal method for returning the value of an option
     */
    std::vector< std::string > get_option_list(
      const char short_name,
      const std::string long_name ) const;

    /**
     * Internal method for returning the value of an option
     */
    std::string get_option(
      const char short_name,
      const std::string long_name,
      const unsigned int index = 0 ) const;

    /**
     * Returns a reference to a flag struct or NULL if it is not found
     */
    flag* find_flag( const char short_name );

    /**
     * Returns a reference to a flag struct or NULL if it is not found
     */
    flag* find_flag( const std::string long_name );

    /**
     * Returns a reference to an option struct or NULL if it is not found
     */
    option* find_option( const char short_name );

    /**
     * Returns a reference to an option struct or NULL if it is not found
     */
    option* find_option( const std::string long_name );

    /**
     * Internal method for processing arguments passed by the command line
     */
    bool process_arguments();

    /**
     * Internal method for processing arguments passed in configuration files
     */
    bool process_config_file( const std::string filename );

    /**
     * The name of the executable for which options are being parsed (set by the constructor)
     */
    std::string executable_name;

    /**
     * Whether or not the usage message has already been displayed
     */
    bool usage_printed;

    /**
     * A list of all inputs indexed by name
     */
    std::map< std::string, std::string > input_map;

    /**
     * A list of arguments indexed by sequence
     */
    std::vector< std::string > argument_list;

    /**
     * A list of all configuration files
     */
    std::vector< std::string > config_file_list;

    /**
     * A list of all flags indexed by sequence
     */
    std::vector< flag > flag_list;

    /**
     * A list of all options indexed by sequence
     */
    std::vector< option > option_list;

    /**
     * A list of all lines in the usage message indexed by sequence
     */
    std::vector< std::string > usage_list;

    /**
     * Divides a string by the provided separator, returning the results as a vector of strings
     */
    inline static std::vector< std::string > explode( std::string str, std::string separator )
    {
      std::vector< std::string > results;
      int found = str.find_first_of( separator );
      while( found != std::string::npos )
      {
        if( found > 0 ) results.push_back( str.substr( 0, found ) );
        str = str.substr( found + 1 );
        found = str.find_first_of( separator );
      }
      if( str.length() > 0 ) results.push_back( str );
      return results;
    }

    /**
     * Removes all space characters (as defined by std::isspace) from the left side of a string
     */
    inline static std::string &ltrim( std::string &s )
    {
      s.erase(
        s.begin(), std::find_if(
          s.begin(), s.end(), std::not1(
            std::ptr_fun<int, int>( std::isspace ) ) ) );
      return s;
    }

    /**
     * Removes all space characters (as defined by std::isspace) from the right side of a string
     */
    inline static std::string &rtrim( std::string &s )
    {
      s.erase(
        std::find_if(
          s.rbegin(), s.rend(), std::not1(
            std::ptr_fun<int, int>( std::isspace ) ) ).base(), s.end() );
      return s;
    }

    /**
     * Removes all space characters (as defined by std::isspace) from both sides of a string
     */
    inline static std::string &trim(std::string &s)
    {
      return ltrim(rtrim(s));
    }

  };
//}

/** @} end of doxygen group */

#endif
