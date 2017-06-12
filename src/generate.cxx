/*=========================================================================

  Program:  sampsim
  Module:   generate.cxx
  Language: C++

=========================================================================*/
//
// .SECTION Description
// An executable which generates a population
//

#include "arc_epi_sample.h"
#include "circle_gps_sample.h"
#include "random_sample.h"
#include "square_gps_sample.h"
#include "strip_epi_sample.h"

#include "common.h"
#include "options.h"
#include "population.h"
#include "town.h"
#include "trend.h"
#include "utilities.h"

#include <stdexcept>

// main function
int main( const int argc, const char** argv )
{
  int status = EXIT_FAILURE;
  sampsim::options opts( argv[0] );
  sampsim::population *population = new sampsim::population;
  sampsim::sample::arc_epi *arc_epi_sample = new sampsim::sample::arc_epi;
  sampsim::sample::circle_gps *circle_gps_sample = new sampsim::sample::circle_gps;
  sampsim::sample::random *random_sample = new sampsim::sample::random;
  sampsim::sample::square_gps *square_gps_sample = new sampsim::sample::square_gps;
  sampsim::sample::strip_epi *strip_epi_sample = new sampsim::sample::strip_epi;

  // define inputs
  opts.add_input( "output" );

  // define general parameters
  opts.add_flag( 'f', "flat_file", "Whether to output data in two CSV files instead of JSON data" );
  if( GNUPLOT_AVAILABLE )
    opts.add_flag( 'p', "plot", "Whether to create a plot of the population (will create a flat-file)" );
  opts.add_flag( 'v', "verbose", "Be verbose when generating population" );

  // define population parameters
  opts.add_heading( "" );
  opts.add_heading( "Global population parameters:" );
  opts.add_heading( "" );
  opts.add_option( "seed", "", "Seed used by the random generator" );
  opts.add_option( "towns", "1", "Number of towns to generate" );
  opts.add_option( "town_size_min", "10000", "The minimum number of individuals in a town" );
  opts.add_option( "town_size_max", "1000000", "The maximum number of individuals in a town" );
  opts.add_option( "town_size_shape", "1.0", "The shape parameter used by the town size Parato distribution" );
  opts.add_option( "tile_x", "1", "Number of tiles in the horizontal direction" );
  opts.add_option( "tile_y", "1", "Number of tiles in the vertical direction" );
  opts.add_option( "tile_width", "1", "Width of a tile in kilometers" );
  opts.add_option( "popdens_mx", "0", "Population density trend's X coefficient (must be [-1,1])" );
  opts.add_option( "popdens_my", "0", "Population density trend's Y coefficient (must be [-1,1])" );
  opts.add_option( "mean_household_pop", "4", "Mean number of individuals per household" );
  opts.add_option( "river_probability", "0", "The probability that a town has a river (must be [0,1])" );
  opts.add_option( "river_width", "0", "How wide to make rivers, in meters (must be smaller than tile width)" );
  opts.add_option( "disease_pockets", "0", "Number of disease pockets to generate" );
  opts.add_option( "pocket_kernel_type", "exponential", "The type of kernel to use for disease pockets" );
  opts.add_option( "pocket_scaling", "1", "The scaling factor to use for disease pocket" );
  opts.add_heading( "" );
  opts.add_heading( "Population trends:" );
  opts.add_heading( "" );
  opts.add_heading( "  All population trends are defined as equations in the form:" );
  opts.add_heading( "" );
  opts.add_heading( "      F(x,y) = b00 + b01*x + b10*y + b02*x^2 + b20*y^2 + b11*x*y" );
  opts.add_heading( "" );
  opts.add_heading( "  The coefficients, b00, b01, etc, are generated for each town using a linear regression coefficient" );
  opts.add_heading( "  (which varies with the log of the town's population) and a residual variance:" );
  opts.add_heading( "" );
  opts.add_heading( "      B(s) = Normal( b + S( log(N) - mean(log(N)) ), σ^2 )" );
  opts.add_heading( "" );
  opts.add_heading( "  where b is the default value, S is the slope, N is the town's population size, and σ is the" );
  opts.add_heading( "  standard deviation." );
  opts.add_heading( "" );
  opts.add_heading( "  Each coefficient's parameter may either be defined by a single number or three numbers delineated by" );
  opts.add_heading( "  a comma and without spaces (ex: 2.0 or 2.0,1.0,0.5).  If a single number is used then the base value" );
  opts.add_heading( "  (for which the town with the mean population will have) will be set to this value and the coefficient" );
  opts.add_heading( "  will not vary across towns.  If three numbers are defined then the base value will be set to the" );
  opts.add_heading( "  first, the linear regression coefficient to the second and the residual variance to the third." );
  opts.add_heading( "" );

  static const std::string array_00[] = { "1", "0", "0" };
  std::vector< std::string > vector_00( array_00, array_00 + sizeof( array_00 ) / sizeof( array_00[0] ) );
  static const std::string array_xx[] = { "0", "0", "0" };
  std::vector< std::string > vector_xx( array_xx, array_xx + sizeof( array_xx ) / sizeof( array_xx[0] ) );

  opts.add_option( "mean_income_b00", vector_00, "Mean income trend's independent coefficient base value" );
  opts.add_option( "mean_income_b01", vector_xx, "Mean income trend's X coefficient base value" );
  opts.add_option( "mean_income_b10", vector_xx, "Mean income trend's Y coefficient base value" );
  opts.add_option( "mean_income_b02", vector_xx, "Mean income trend's X^2 coefficient base value" );
  opts.add_option( "mean_income_b20", vector_xx, "Mean income trend's Y^2 coefficient base value" );
  opts.add_option( "mean_income_b11", vector_xx, "Mean income trend's XY coefficient base value" );
  opts.add_option( "sd_income_b00", vector_00, "SD income trend's independent coefficient base value" );
  opts.add_option( "sd_income_b01", vector_xx, "SD income trend's X coefficient base value" );
  opts.add_option( "sd_income_b10", vector_xx, "SD income trend's Y coefficient base value" );
  opts.add_option( "sd_income_b02", vector_xx, "SD income trend's X^2 coefficient base value" );
  opts.add_option( "sd_income_b20", vector_xx, "SD income trend's Y^2 coefficient base value" );
  opts.add_option( "sd_income_b11", vector_xx, "SD income trend's XY coefficient base value" );
  opts.add_option( "mean_disease_b00", vector_00, "Mean disease trend's independent coefficient base value" );
  opts.add_option( "mean_disease_b01", vector_xx, "Mean disease trend's X coefficient base value" );
  opts.add_option( "mean_disease_b10", vector_xx, "Mean disease trend's Y coefficient base value" );
  opts.add_option( "mean_disease_b02", vector_xx, "Mean disease trend's X^2 coefficient base value" );
  opts.add_option( "mean_disease_b20", vector_xx, "Mean disease trend's Y^2 coefficient base value" );
  opts.add_option( "mean_disease_b11", vector_xx, "Mean disease trend's XY coefficient base value" );
  opts.add_option( "sd_disease_b00", vector_00, "SD disease trend's independent coefficient base value" );
  opts.add_option( "sd_disease_b01", vector_xx, "SD disease trend's X coefficient base value" );
  opts.add_option( "sd_disease_b10", vector_xx, "SD disease trend's Y coefficient base value" );
  opts.add_option( "sd_disease_b02", vector_xx, "SD disease trend's X^2 coefficient base value" );
  opts.add_option( "sd_disease_b20", vector_xx, "SD disease trend's Y^2 coefficient base value" );
  opts.add_option( "sd_disease_b11", vector_xx, "SD disease trend's XY coefficient base value" );

  // define disease parameters
  opts.add_heading( "" );
  opts.add_heading( "Disease status weighting parameters:" );
  opts.add_heading( "" );
  opts.add_option( "dweight_population", "1.0", "Disease weight for household population" );
  opts.add_option( "dweight_income", "1.0", "Disease weight for household income" );
  opts.add_option( "dweight_risk", "1.0", "Disease weight for household risk" );
  opts.add_option( "dweight_age", "1.0", "Disease weight for household age" );
  opts.add_option( "dweight_sex", "1.0", "Disease weight for household sex" );
  opts.add_option( "dweight_pocket", "1.0", "Disease weight for pocketing" );

  // define batch parameters
  opts.add_heading( "" );
  opts.add_heading( "Batch operation parameters:" );
  opts.add_heading( "" );
  opts.add_option( "batch_sampler", "", "Which sampler to use when batch processing" );
  opts.add_option( "batch_config", "", "Config file containing options to pass to the batch sampler" );
  opts.add_option( "batch_npop", "1", "Number of populations to generate" );
  opts.add_option( "batch_nsamp", "0", "Number of samples to take of each population" );

  try
  {
    // parse the command line arguments
    opts.set_arguments( argc, argv );
    if( opts.process() )
    {
      // now either show the help or run the application
      if( opts.get_flag( "help" ) )
      {
        opts.print_usage();
      }
      else
      {
        std::string filename = opts.get_input( "output" );
        sampsim::utilities::verbose = opts.get_flag( "verbose" );

        // work out the batch job details, if requested
        std::string batch_sampler = opts.get_option( "batch_sampler" );
        std::string batch_config = opts.get_option( "batch_config" );
        int batch_npop = opts.get_option_as_int( "batch_npop" );
        int batch_nsamp = opts.get_option_as_int( "batch_nsamp" );
        double river_width = opts.get_option_as_double( "river_width" ) / 1000;
        double tile_width = opts.get_option_as_double( "tile_width" );

        if( 0 < batch_sampler.length() && 0 >= batch_nsamp )
        {
          std::cout << "ERROR: Batch sampler provided without specifying how many samples to take."
                    << std::endl
                    << "       Make sure to set batch_nsamp when providing a batch_sampler."
                    << std::endl;
        }
        else if( 0 < batch_config.length() && 0 >= batch_nsamp )
        {
          std::cout << "ERROR: Batch config provided without specifying how many samples to take. "
                    << std::endl
                    << "       Make sure to set batch_nsamp when providing a batch_config."
                    << std::endl;
        }
        else if( 0 < batch_config.length() && 0 == batch_sampler.length() )
        {
          std::cout << "ERROR: Batch config provided without specifying which sampler to use. "
                    << std::endl
                    << "       Make sure to set batch_sampler when providing a batch_config."
                    << std::endl;
        }
        else if( 0 < batch_nsamp && 0 == batch_sampler.length() )
        {
          std::cout << "ERROR: Number of batch samples provided without specifying which sampler to use. "
                    << std::endl
                    << "       Make sure to set batch_sampler when providing batch_nsamp."
                    << std::endl;
        }
        else if( 0 >= tile_width )
        {
          std::cout << "ERROR: Tile width must be > 0. "
                    << std::endl
                    << "       Make sure to set a non-negative value for tile_width."
                    << std::endl;
        }
        else if( river_width >= tile_width )
        {
          std::cout << "ERROR: Tile width must be larger than river width. "
                    << std::endl
                    << "       Make sure to set tile_width > river_width."
                    << std::endl;
        }
        else
        {
          std::cout << "sampsim generate version " << sampsim::utilities::get_version() << std::endl;

          population->set_seed( opts.get_option( "seed" ) );
          population->set_number_of_towns( opts.get_option_as_int( "towns" ) );
          population->set_town_size_min( opts.get_option_as_double( "town_size_min" ) );
          population->set_town_size_max( opts.get_option_as_double( "town_size_max" ) );
          population->set_town_size_shape( opts.get_option_as_double( "town_size_shape" ) );
          population->set_mean_household_population( opts.get_option_as_double( "mean_household_pop" ) );
          population->set_number_of_tiles_x( opts.get_option_as_int( "tile_x" ) );
          population->set_number_of_tiles_y( opts.get_option_as_int( "tile_y" ) );
          population->set_tile_width( tile_width );
          population->set_river_probability( opts.get_option_as_double( "river_probability" ) );
          population->set_river_width( river_width );
          population->set_population_density_slope(
            opts.get_option_as_double( "popdens_mx" ),
            opts.get_option_as_double( "popdens_my" ) );
          population->set_disease_weights(
            opts.get_option_as_double( "dweight_population" ),
            opts.get_option_as_double( "dweight_income" ),
            opts.get_option_as_double( "dweight_risk" ),
            opts.get_option_as_double( "dweight_age" ),
            opts.get_option_as_double( "dweight_sex" ),
            opts.get_option_as_double( "dweight_pocket" ) );
          population->set_number_of_disease_pockets( opts.get_option_as_int( "disease_pockets" ) );
          population->set_pocket_kernel_type( opts.get_option( "pocket_kernel_type" ) );
          population->set_pocket_scaling( opts.get_option_as_double( "pocket_scaling" ) );

          // build trends
          sampsim::trend *mean_income = population->get_mean_income();
          mean_income->set_b00( opts.get_option_as_double_list( "mean_income_b00" ) );
          mean_income->set_b01( opts.get_option_as_double_list( "mean_income_b01" ) );
          mean_income->set_b10( opts.get_option_as_double_list( "mean_income_b10" ) );
          mean_income->set_b02( opts.get_option_as_double_list( "mean_income_b02" ) );
          mean_income->set_b20( opts.get_option_as_double_list( "mean_income_b20" ) );
          mean_income->set_b11( opts.get_option_as_double_list( "mean_income_b11" ) );
          sampsim::trend *sd_income = population->get_sd_income();
          sd_income->set_b00( opts.get_option_as_double_list( "sd_income_b00" ) );
          sd_income->set_b01( opts.get_option_as_double_list( "sd_income_b01" ) );
          sd_income->set_b10( opts.get_option_as_double_list( "sd_income_b10" ) );
          sd_income->set_b02( opts.get_option_as_double_list( "sd_income_b02" ) );
          sd_income->set_b20( opts.get_option_as_double_list( "sd_income_b20" ) );
          sd_income->set_b11( opts.get_option_as_double_list( "sd_income_b11" ) );

          sampsim::trend *mean_disease = population->get_mean_disease();
          mean_disease->set_b00( opts.get_option_as_double_list( "mean_disease_b00" ) );
          mean_disease->set_b01( opts.get_option_as_double_list( "mean_disease_b01" ) );
          mean_disease->set_b10( opts.get_option_as_double_list( "mean_disease_b10" ) );
          mean_disease->set_b02( opts.get_option_as_double_list( "mean_disease_b02" ) );
          mean_disease->set_b20( opts.get_option_as_double_list( "mean_disease_b20" ) );
          mean_disease->set_b11( opts.get_option_as_double_list( "mean_disease_b11" ) );
          sampsim::trend *sd_disease = population->get_sd_disease();
          sd_disease->set_b00( opts.get_option_as_double_list( "sd_disease_b00" ) );
          sd_disease->set_b01( opts.get_option_as_double_list( "sd_disease_b01" ) );
          sd_disease->set_b10( opts.get_option_as_double_list( "sd_disease_b10" ) );
          sd_disease->set_b02( opts.get_option_as_double_list( "sd_disease_b02" ) );
          sd_disease->set_b20( opts.get_option_as_double_list( "sd_disease_b20" ) );
          sd_disease->set_b11( opts.get_option_as_double_list( "sd_disease_b11" ) );

          std::string population_filename;
          for( int p = 0; p < batch_npop; p++ )
          {
            // filename depends on whether we are creating a batch of populations or not
            if( 1 < batch_npop )
            {
              std::stringstream stream;
              stream << filename << ".p"
                     << std::setw( log( batch_npop+1 ) ) << std::setfill( '0' ) << p;
              population_filename = stream.str();
              std::cout << "generating population " << ( p+1 ) << " of " << batch_npop << std::endl;
            }
            else population_filename = filename;

            population->generate();

            bool flat = opts.get_flag( "flat_file" );
            bool plot = GNUPLOT_AVAILABLE ? opts.get_flag( "plot" ) : false;

            // create a json file no flat file was requested
            if( !flat ) population->write( population_filename, false );
            
            // create a flat file if a flat file or plot was requested
            if( flat || plot ) population->write( population_filename, true );

            if( 0 == batch_nsamp )
            {
              // plot the flat file if requested to
              if( plot )
              {
                std::stringstream stream;
                unsigned int index = 0;
                for( auto it = population->get_town_list_cbegin();
                     it != population->get_town_list_cend();
                     ++it, ++index )
                {
                  sampsim::town *town = *it;
                  std::string result = sampsim::utilities::exec(
                    gnuplot( town, population_filename, index ) );

                  stream.str( "" );
                  stream << population_filename << ".t"
                         << std::setw( log( population->get_number_of_towns()+1 ) ) << std::setfill( '0' )
                         << index << ".png";
                  std::string image_filename = stream.str();

                  stream.str( "" );
                  std::stringstream stream;
                  if( "ERROR" == result )
                    stream << "warning: failed to create plot";
                  else
                    stream << "creating plot file \"" << image_filename << "\"";
                  sampsim::utilities::output( stream.str() );
                }
              }
            }
            else
            {
              // determine which sampler to use and set up the options for it
              const char* sampler_argv[3];
              sampler_argv[0] = batch_sampler.c_str();
              sampler_argv[1] = "--config";
              sampsim::options sampler_opts( argv[0] );
              sampler_opts.add_option( "seed", "", "Seed used by the random generator" );
              sampsim::sample::sample *sample;
              if( "arc_epi" == batch_sampler || "arc_epi_sample" == batch_sampler )
              {
                setup_arc_epi_sample( sampler_opts );
                if( 0 < batch_config.length() )
                {
                  sampler_argv[2] = batch_config.c_str();
                  sampler_opts.set_arguments( 3, sampler_argv );
                }
                if( !sampler_opts.process() ) throw std::runtime_error( "Error while setting up sampler" );
                parse_arc_epi_sample( sampler_opts, arc_epi_sample );
                arc_epi_sample->set_population( population );
                sample = arc_epi_sample;
              }
              else if( "circle_gps" == batch_sampler || "circle_gps_sample" == batch_sampler )
              {
                setup_circle_gps_sample( sampler_opts );
                if( 0 < batch_config.length() )
                {
                  sampler_argv[2] = batch_config.c_str();
                  sampler_opts.set_arguments( 3, sampler_argv );
                }
                if( !sampler_opts.process() ) throw std::runtime_error( "Error while setting up sampler" );
                parse_circle_gps_sample( sampler_opts, circle_gps_sample );
                circle_gps_sample->set_population( population );
                sample = circle_gps_sample;
              }
              else if( "random" == batch_sampler || "random_sample" == batch_sampler )
              {
                setup_random_sample( sampler_opts );
                if( 0 < batch_config.length() )
                {
                  sampler_argv[2] = batch_config.c_str();
                  sampler_opts.set_arguments( 3, sampler_argv );
                }
                if( !sampler_opts.process() ) throw std::runtime_error( "Error while setting up sampler" );
                parse_random_sample( sampler_opts, random_sample );
                random_sample->set_population( population );
                sample = random_sample;
              }
              else if( "square_gps" == batch_sampler || "square_gps_sample" == batch_sampler )
              {
                setup_square_gps_sample( sampler_opts );
                if( 0 < batch_config.length() )
                {
                  sampler_argv[2] = batch_config.c_str();
                  sampler_opts.set_arguments( 3, sampler_argv );
                }
                if( !sampler_opts.process() ) throw std::runtime_error( "Error while setting up sampler" );
                parse_square_gps_sample( sampler_opts, square_gps_sample );
                square_gps_sample->set_population( population );
                sample = square_gps_sample;
              }
              else if( "strip_epi" == batch_sampler || "strip_epi_sample" == batch_sampler )
              {
                setup_strip_epi_sample( sampler_opts );
                if( 0 < batch_config.length() )
                {
                  sampler_argv[2] = batch_config.c_str();
                  sampler_opts.set_arguments( 3, sampler_argv );
                }
                if( !sampler_opts.process() ) throw std::runtime_error( "Error while setting up sampler" );
                parse_strip_epi_sample( sampler_opts, strip_epi_sample );
                strip_epi_sample->set_population( population );
                sample = strip_epi_sample;
              }
              else
              {
                std::stringstream stream;
                stream << "Unknown sampler \"" << batch_sampler << "\", must be the same as one of "
                       << "the executables ending in _sample";
                throw std::runtime_error( stream.str() );
              }

              std::string sample_filename;
              for( int s = 0; s < batch_nsamp; s++ )
              {
                // filename depends on whether we are creating a batch of samples or not
                sample_filename = population_filename + "_sample";
                if( 1 < batch_nsamp )
                {
                  std::stringstream stream;
                  stream << population_filename << ".s"
                         << std::setw( log( batch_nsamp+1 ) ) << std::setfill( '0' ) << s;
                  sample_filename = stream.str();
                  std::cout << "sampling iteration " << ( s+1 ) << " of " << batch_nsamp << std::endl;
                }

                sample->generate();

                // create a json file no flat file was requested
                if( !flat ) sample->write( sample_filename, false );

                // create a flat file if a flat file or plot was requested
                if( flat || plot ) sample->write( sample_filename, true );

                // plot the flat file if requested to
                if( plot )
                {
                  std::stringstream stream;
                  unsigned int index = 0;
                  for( auto it = population->get_town_list_cbegin();
                       it != population->get_town_list_cend();
                       ++it, ++index )
                  {
                    sampsim::town *town = *it;
                    std::string result = sampsim::utilities::exec(
                      gnuplot( town, population_filename, index, sample_filename ) );

                    stream.str( "" );
                    stream << sample_filename << ".t"
                           << std::setw( log( population->get_number_of_towns()+1 ) ) << std::setfill( '0' )
                           << index << ".png";
                    std::string image_filename = stream.str();

                    stream.str( "" );
                    std::stringstream stream;
                    if( "ERROR" == result )
                      stream << "warning: failed to create plot";
                    else
                      stream << "creating plot file \"" << image_filename << "\"";
                    sampsim::utilities::output( stream.str() );
                  }
                }
              }
            }
          }
        }
      }

      status = EXIT_SUCCESS;
    }
  }
  catch( std::exception &e )
  {
    std::cerr << "Uncaught exception: " << e.what() << std::endl;
  }

  sampsim::utilities::safe_delete( population );
  sampsim::utilities::safe_delete( arc_epi_sample );
  sampsim::utilities::safe_delete( circle_gps_sample );
  sampsim::utilities::safe_delete( random_sample );
  sampsim::utilities::safe_delete( square_gps_sample );
  sampsim::utilities::safe_delete( strip_epi_sample );
  return status;
}
