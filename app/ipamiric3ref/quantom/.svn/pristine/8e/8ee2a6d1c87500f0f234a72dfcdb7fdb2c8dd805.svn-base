
#ifndef CONFIGURE_HPP
#define CONFIGURE_HPP

namespace quantom
{
  class configure 
  {
  public:

	// set default configuration
	configure ( void ) :
	  c_lubyShift(12),
	  c_innerPico(1024),
	  c_outerPico(32),
	  c_ip(2),
	  c_decHeu(3),
	  c_restartHeu(1),
	  c_svs(true),
	  c_cacheSwap(true),
	  c_lhbr(false),
	  c_prepro(false),
	  c_inpro(false),
	  c_subsumption(false),
	  c_upla(false),
	  c_varElim(false),
	  c_quantMerge(false),
	  c_maxMode(1),
	  c_maxInpro(true),
	  c_solvercalls(0),
	  c_maxsolvercalls(0),
	  c_results(0),
	  c_maxresults(0),
	  c_runtime(0.0),
	  c_active(false)
	{} 

	configure ( unsigned int lubyShift, unsigned int innerPico, unsigned int outerPico, unsigned int ip, unsigned int decHeu, unsigned int restartHeu, unsigned int svs, 
				bool cacheSwap, bool lhbr, bool prepro, bool inpro, bool subsumption, bool upla, bool varElim, bool quantMerge, unsigned int maxMode, bool maxInpro, bool active ) :
	  c_lubyShift(lubyShift),
	  c_innerPico(innerPico),
	  c_outerPico(outerPico),
	  c_ip(ip),
	  c_decHeu(decHeu),
	  c_restartHeu(restartHeu),
	  c_svs(svs),
	  c_cacheSwap(cacheSwap),
	  c_lhbr(lhbr),
	  c_prepro(prepro),
	  c_inpro(inpro),
	  c_subsumption(subsumption),
	  c_upla(upla),
	  c_varElim(varElim),
	  c_quantMerge(quantMerge),
	  c_maxMode(maxMode),
	  c_maxInpro(maxInpro),
	  c_solvercalls(0),
	  c_maxsolvercalls(0),
	  c_results(0),
	  c_maxresults(0),
	  c_runtime(0.0),
	  c_active(active)
	{} 

	~configure() {};

	/* Copy Constructors */
	configure ( const configure& config ) :
	  c_lubyShift(config.c_lubyShift),
	  c_innerPico(config.c_innerPico),
	  c_outerPico(config.c_outerPico),
	  c_ip(config.c_ip),
	  c_decHeu(config.c_decHeu),
	  c_restartHeu(config.c_restartHeu),
	  c_svs(config.c_svs),
	  c_cacheSwap(config.c_cacheSwap),
	  c_lhbr(config.c_lhbr),
	  c_prepro(config.c_prepro),
	  c_inpro(config.c_inpro),
	  c_subsumption(config.c_subsumption),
	  c_upla(config.c_upla),
	  c_varElim(config.c_varElim),
	  c_quantMerge(config.c_quantMerge),
	  c_maxMode(config.c_maxMode),
	  c_maxInpro(config.c_maxInpro),
	  c_solvercalls(config.c_solvercalls),
	  c_maxsolvercalls(config.c_maxsolvercalls),
	  c_results(config.c_results),
	  c_maxresults(config.c_maxresults),
	  c_runtime(config.c_runtime),
	  c_active(config.c_active)
	{ }

	configure& operator= ( const configure& config )
	{
	  c_lubyShift = config.c_lubyShift;
	  c_innerPico = config.c_innerPico;
	  c_outerPico = config.c_outerPico;
	  c_ip = config.c_ip;
	  c_decHeu = config.c_decHeu;
	  c_restartHeu =config.c_restartHeu;
	  c_svs = config.c_svs;
	  c_cacheSwap = config.c_cacheSwap;
	  c_lhbr = config.c_lhbr;
	  c_prepro = config.c_prepro;
	  c_inpro = config.c_inpro;
	  c_subsumption = config.c_subsumption;
	  c_upla = config.c_upla;
	  c_varElim = config.c_varElim;
	  c_quantMerge = config.c_quantMerge;
	  c_maxMode = config.c_maxMode;
	  c_maxInpro = config.c_maxInpro;
	  c_solvercalls = config.c_solvercalls;
	  c_maxsolvercalls = config.c_maxsolvercalls;
	  c_results = config.c_results;
	  c_maxresults = config.c_maxresults;
	  c_runtime = config.c_runtime;
	  c_active = config.c_active;
	  return *this;
	}

	// Sets the heuristic settings to (guided) random values
	void randomizeSetting ( void )
	{
	  c_lubyShift = 6 + ( rand() % 18 );
	  c_innerPico = 256 + (256* (rand()%16) );
	  c_outerPico = 8 + (8* (rand()%16) );
	  c_ip = rand()%4;
	  c_decHeu = rand()%4;
	  c_restartHeu = rand()%4;
	  c_svs = rand()%2;
	  c_cacheSwap = rand()%2;
	  c_lhbr = rand()%2;
	  c_prepro = rand()%2;
	  c_inpro = rand()%2;
	  c_subsumption = rand()%2;
	  c_upla = rand()%2;
	  c_varElim = rand()%2;
	  c_quantMerge = rand()%2;
	  c_maxMode = rand()%3;
	  c_maxInpro = rand()%2;
	}

	/* maybe later... VERY sophisticated */
	void loadConfiguationfromFile ( /*std::ostream& file*/ ) { assert(false); }

	// Luby shift (only considered for "c_restartHeu=1")
	// Suggested interval: [6,18]
	unsigned int c_lubyShift;

	// PicoSAT constansts (only considered for "c_restartHeu=2")
	// Suggestes interval: [256, 4096] (in steps of 256)
	unsigned int c_innerPico;
	// Suggestes interval: [8, 128] (in steps of 8)
	unsigned int c_outerPico;

	// Initial Phase for variables 
    // 0: FALSE
    // 1: All flipped (Phase depends also of decision heuristic)
	// 2: Universal flipped (default)
	// 3: Random
	unsigned int c_ip;

	// Decision Heuristic
	// 0: VSIDS
	// 1: SLCS (static literal combination sum)
	// 2: DLCS (dynamic literal combination sum)
	// 3: QuBE-Style (default)
	unsigned int c_decHeu;

	// Restart Heuristic
	// 0: no restarts
	// 1: luby-style (default)
	// 2: picosat-style (inner and outer loops)
	// 3: glucose-style (average decision level)
	unsigned int c_restartHeu;

   	// Switch cache value of variable assignment with restart (default: true)
	bool c_svs;

	// Swap cached due to pos and neg occurence (default: true)
	bool c_cacheSwap;

	// Lazy Hyper Binary Resolution (default: false)
	bool c_lhbr;

  
	// Use preprocessor (default: false)
	bool c_prepro;

	// Use inprocessor (default: false)
	bool c_inpro;

	// Use subsumption in pre-/in-processor (default: false)
	bool c_subsumption;

	// Use UPLA in pre-/in-processor (default: false)
	bool c_upla;

	// Use variable elimination in pre-/in-processor (default: false)
	bool c_varElim;

	// Use quantifier merging in pre-/in-processor (default: false)
	bool c_quantMerge;

	// Search modi for MaxQBF
	// 0: unsatisfiability based
	// 1: satisfiability based (default)
	// 2: binary search based
	unsigned int c_maxMode;

	// Inprocessing during maxQBF?
	bool c_maxInpro;

	// Counts the solver calls for this configuration
	unsigned int c_solvercalls;
	unsigned int c_maxsolvercalls;

	// Counts the solver calls with result != QUANTOM_UNKNOWN
	unsigned int c_results;
	unsigned int c_maxresults;

	// Accumulated runtime for this configuration
	double c_runtime;

	bool c_active;

  };

   class configureset 
  {

  public:

	/* constructor */
	configureset ( void ) :
	  configurations(),
	  doMaxQBF(false),
	  initconfigs(1)
	{
	  configurations.resize(8);
	  configurations[0] = new configure(); 
	  configurations[1] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, true, false, true, true, true, true, 1, true, false); 
	  configurations[2] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, true, true, true, true, true, true, 1, true, false); 
	  configurations[3] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, false, true, true, true, true, true, 1, true, false); 
	  configurations[4] = new configure(12, 1024, 32, 2, 3, 0, true, true, false, true, false, false, true, true, false, 1, true, false); 
	  configurations[5] = new configure(12, 1024, 32, 0, 2, 3, true, true, false, false, false, false, false, false, false, 1, true, false); 
	  configurations[6] = new configure(15, 1024, 32, 1, 1, 1, true, false, false, true, false, false, true, true, false, 1, true, false); 
	  configurations[7] = new configure(12, 512, 24, 0, 2, 2, false, true, false, false, false, false, false, false, false, 1, true, false); 
	};

	configureset ( unsigned int instances ) :
	  configurations(),
	  doMaxQBF(false),
	  initconfigs(instances)
	{
	  assert( instances > 0 );
	  configurations.resize(8);

	  // Some predefined settings
	  configurations[0] = new configure(); 
	  configurations[1] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, true, false, true, true, true, true, 1, true, (instances>1)); 
	  configurations[2] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, true, true, true, true, true, true, 1, true, (instances>2)); 
	  configurations[3] = new configure(12, 1024, 32, 2, 3, 1, true, true, false, false, true, true, true, true, true, 1, true, (instances>3)); 
	  configurations[4] = new configure(12, 1024, 32, 2, 3, 0, true, true, false, true, false, false, true, true, false, 1, true, (instances>4)); 
	  configurations[5] = new configure(12, 1024, 32, 0, 2, 3, true, true, false, false, false, false, false, false, false, 1, true, (instances>5)); 
	  configurations[6] = new configure(15, 1024, 32, 1, 1, 1, true, false, false, true, false, false, true, true, false, 1, true, (instances>6)); 
	  configurations[7] = new configure(12, 512, 24, 0, 2, 2, false, true, false, false, false, false, false, false, false, 1, true, (instances>7)); 

	  if( instances > 8 )
		{
		  // fill last configs with randomized configs
		  configurations.resize(instances);
		  for( unsigned int i = 8; i != instances; ++i )
			{ 
			  configurations[i] = new configure(); 
			  configurations[i]->randomizeSetting();
			}
		}
	}

	~configureset() 
	{
	  for( unsigned int i = 0; i != configurations.size(); ++i )
		{ delete configurations[i]; }
	};

	unsigned int size( void ) const
	{ return configurations.size(); }

	void resize( unsigned int size )
	{ 
	  configurations.resize(size);

	  for( unsigned int i = 0; i != size; ++i )
		{
		  if( configurations[i] == NULL )
			{
			  configurations[i] = new configure;
			  configurations[i]->randomizeSetting();
			}
		}
	}

	std::vector< configure* >::iterator begin ( void )
	{ return configurations.begin(); }

	std::vector< configure* >::iterator end ( void )
	{ return configurations.end(); }

	quantom::configure* operator[]( size_t pos ) const
	{ assert( pos < configurations.size()); return configurations[pos]; }

	quantom::configure* operator[](size_t pos )
	{ assert( pos < configurations.size()); return configurations[pos]; }

	// Add "additionalSettings" new random settings
	void addRandomSettings ( unsigned int additionalSettings )
	{
	  assert( additionalSettings > 0 );

	  unsigned int totalsettings = initconfigs + additionalSettings;
	  configurations.resize( totalsettings );
	  ++initconfigs;
	  
	  while( initconfigs < configurations.size() )
		{
		  configurations[initconfigs]->randomizeSetting();
		  configurations[initconfigs]->c_active = true;
		  ++initconfigs;
		}
	  
	  while( configurations.size() < totalsettings )
		{
		  configure* config = new configure(); 
		  config->randomizeSetting();
		  config->c_active = true;
		  addSetting ( *config );
		  ++initconfigs;
		}
	}

	// Add a defined configuration "config"
	void addSetting ( configure config )
	{
	  configurations.push_back( &config );
	  if( config.c_active )
		{ ++initconfigs; }
	}

	// Set a certain configuration to "config"
	void setSetting ( configure config, unsigned int id )
	{
	  assert( id < configurations.size() ); 
	  configurations[id] = &config;
	}

	// Randomize setting "id"
	void randomizeSetting ( unsigned int id ) 
	{ 
	  assert( id < configurations.size() ); 
	  configurations[id]->randomizeSetting();
	}

	// (De-)activates setting "id"
	void activateSetting( bool val, unsigned int id )
	{
	  assert( id < configurations.size() ); 
	  configurations[id]->c_active = val;
	  if( id > initconfigs )
		{ initconfigs = id; }
	}

	void applyMaxQBF ( bool val ) { doMaxQBF = val; }

	void setBlackMagicMode( unsigned int val ) 
	{ 
	  assert( val > 0 );
	  initconfigs = val;

	  configurations.resize(val);
	  for( unsigned int i = 0; i != val; ++i )
		{
		  assert( configurations[i] != NULL );
		  configurations[i]->c_active = true; 
		}
	}

	void printSetting ( unsigned int id ) const
	{
	  assert( id < configurations.size() ); 

	  std::cout << "-----------------------------" << std::endl
				<< "print setting for id: " << id << std::endl
				<< "c_lubyShift........ = " << configurations[id]->c_lubyShift << std::endl
				<< "c_innerPico........ = " << configurations[id]->c_innerPico << std::endl
				<< "c_outerPico........ = " << configurations[id]->c_outerPico << std::endl
				<< "c_ip............... = " << configurations[id]->c_ip << std::endl
				<< "c_decHeu........... = " << configurations[id]->c_decHeu << std::endl
				<< "c_restartHeu....... = " << configurations[id]->c_restartHeu << std::endl
				<< "c_svs.............. = " << configurations[id]->c_svs << std::endl
				<< "c_cacheSwap........ = " << configurations[id]->c_cacheSwap << std::endl
				<< "c_lhbr............. = " << configurations[id]->c_lhbr << std::endl
				<< "c_prepro........... = " << configurations[id]->c_prepro << std::endl
				<< "c_inpro............ = " << configurations[id]->c_inpro << std::endl
				<< "c_subsumption...... = " << configurations[id]->c_subsumption << std::endl
				<< "c_upla............. = " << configurations[id]->c_upla << std::endl
				<< "c_varElim.......... = " << configurations[id]->c_varElim << std::endl
				<< "c_quantMerge....... = " << configurations[id]->c_quantMerge << std::endl
				<< "c_maxMode.......... = " << configurations[id]->c_maxMode << std::endl
				<< "c_maxInpro......... = " << configurations[id]->c_maxInpro << std::endl
				<< "c_solvercalls...... = " << configurations[id]->c_solvercalls << std::endl
				<< "c_results.......... = " << configurations[id]->c_results << std::endl
				<< "c_runtime.......... = " << configurations[id]->c_runtime << std::endl
				<< "-----------------------------" << std::endl;
	}

	void dumpSetting ( unsigned int id ) const
	{
	  assert( id < configurations.size() ); 

	  std::cout << "-----------------------------" << std::endl
				<< "Quantom setting for configuration " << id << ":" << std::endl
				<< "(Successfull) solver calls: (" << configurations[id]->c_results << "/" << configurations[id]->c_solvercalls << ") " <<std::endl
				<< "total runtime:  " << configurations[id]->c_runtime << "sec" << std::endl << std::endl
				<< "myQuantom.setDecHeu(" << configurations[id]->c_decHeu << ");" << std::endl
				<< "myQuantom.setRestartHeu("  << configurations[id]->c_restartHeu << ");" << std::endl;

	  if( configurations[id]->c_restartHeu == 1 )
		{ std::cout << "myQuantom.setLubyShift(" << configurations[id]->c_lubyShift  << ");" << std::endl; }
	  if( configurations[id]->c_restartHeu == 2 )
		{ std::cout << "myQuantom.setPicoLoops(" << configurations[id]->c_innerPico << "," << configurations[id]->c_outerPico << ");" << std::endl; } 
	
	  std::cout << "myQuantom.setIP(" << configurations[id]->c_ip << ");" << std::endl
				<< "myQuantom.setSwitchVarSign(" << (configurations[id]->c_svs?"true":"false") << ");" << std::endl
				<< "myQuantom.setCacheSwap(" << (configurations[id]->c_cacheSwap?"true":"false") << ");" << std::endl
				<< "myQuantom.setLHBR(" << (configurations[id]->c_lhbr?"true":"false") << ");" << std::endl << std::endl
				<< "myQuantom.setPreprocessor(" << (configurations[id]->c_prepro?"true":"false") << ");" << std::endl
				<< "myQuantom.setInprocessor(" << (configurations[id]->c_inpro?"true":"false") << ");" << std::endl;

	  if( configurations[id]->c_prepro || configurations[id]->c_inpro || doMaxQBF )
		{
		  std::cout << std::endl << "myQuantom.setSubsumption(" << (configurations[id]->c_subsumption?"true":"false") << ");" << std::endl 
					<< "myQuantom.setUPLA(" << (configurations[id]->c_upla?"true":"false") << ");" << std::endl 
					<< "myQuantom.setVarElim(" << (configurations[id]->c_varElim?"true":"false") << ");" << std::endl 
					<< "myQuantom.setQuantMerge(" << (configurations[id]->c_quantMerge?"true":"false") << ");" << std::endl << std::endl;
		}

	  if( doMaxQBF )
		{
		  std::cout << std::endl << "myQuantom.Maximize( opt," << configurations[id]->c_maxMode << ");" << std::endl; 
		}
	  std::cout << "-----------------------------" << std::endl;
	}

	std::vector< configure* > configurations;

	// Did we apply MaxQBF?
	bool doMaxQBF;

	unsigned int initconfigs;
  };

}
#endif
