#include "./lib/getopt_win.h"
#include "./binary.h"
#include "./processor.h"


namespace phil
{

namespace bin
{


char ACCEPTABLE_OPTIONS[] = "c:e:f:hk:l:m:o:p:t:v:PT:";


std::unique_ptr<lhs_enumerator_library_t, deleter_t<lhs_enumerator_library_t> >
lhs_enumerator_library_t::ms_instance;


lhs_enumerator_library_t* lhs_enumerator_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new lhs_enumerator_library_t());
    return ms_instance.get();
}


lhs_enumerator_library_t::lhs_enumerator_library_t()
{
    add("depth", new lhs::depth_based_enumerator_t::generator_t());
    add("a*", new lhs::a_star_based_enumerator_t::generator_t());
}


std::unique_ptr<ilp_converter_library_t, deleter_t<ilp_converter_library_t> >
ilp_converter_library_t::ms_instance;


ilp_converter_library_t* ilp_converter_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_converter_library_t());
    return ms_instance.get();
}


ilp_converter_library_t::ilp_converter_library_t()
{
    add("null", new cnv::null_converter_t::generator_t());
    add("weighted", new cnv::weighted_converter_t::generator_t());
    add("costed", new cnv::costed_converter_t::generator_t());
}


std::unique_ptr<ilp_solver_library_t, deleter_t<ilp_solver_library_t> >
ilp_solver_library_t::ms_instance;


ilp_solver_library_t* ilp_solver_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_solver_library_t());
    return ms_instance.get();
}


ilp_solver_library_t::ilp_solver_library_t()
{
    add("null", new sol::null_solver_t::generator_t());
    add("lpsolve", new sol::lp_solve_t::generator_t());
    add("gurobi", new sol::gurobi_t::generator_t());
}


std::unique_ptr<distance_provider_library_t, deleter_t<distance_provider_library_t> >
distance_provider_library_t::ms_instance;


distance_provider_library_t* distance_provider_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new distance_provider_library_t());
    return ms_instance.get();
}


distance_provider_library_t::distance_provider_library_t()
{
    add("basic", new kb::dist::basic_distance_provider_t::generator_t());
    add("cost", new kb::dist::cost_based_distance_provider_t::generator_t());
}


std::unique_ptr<category_table_library_t, deleter_t<category_table_library_t> >
category_table_library_t::ms_instance;


category_table_library_t* category_table_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new category_table_library_t());
    return ms_instance.get();
}


category_table_library_t::category_table_library_t()
{
    add("null", new kb::ct::null_category_table_t::generator_t());
    add("basic", new kb::ct::basic_category_table_t::generator_t());
}



bool _load_config_file(
    const char *filename, phillip_main_t *phillip,
    execution_configure_t *option, inputs_t *inputs);

/** @return Validity of input. */
bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
    execution_configure_t *option, inputs_t *inputs);


execution_configure_t::execution_configure_t()
    : mode(EXE_MODE_UNDERSPECIFIED), kb_name("kb.cdb")
{}


void prepare(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
{
    initialize();

    print_console("Phillip starts...");
    print_console("  version: " + phillip_main_t::VERSION);

    parse_options(argc, argv, phillip, config, inputs);
    IF_VERBOSE_1("Phillip has completed parsing comand options.");

    if (config->mode != bin::EXE_MODE_HELP)
        preprocess(*config, phillip);
}


void execute(
    phillip_main_t *phillip,
    const execution_configure_t &config, const inputs_t &inputs)
{
    if (config.mode == bin::EXE_MODE_HELP)
    {
        bin::print_usage();
        return;
    }

    bool do_compile =
        (config.mode == bin::EXE_MODE_COMPILE_KB) or
        phillip->flag("do_compile_kb");

    /* COMPILING KNOWLEDGE-BASE */
    if (do_compile)
    {
        kb::knowledge_base_t *kb = kb::knowledge_base_t::instance();
        proc::processor_t processor;
        print_console("Compiling knowledge-base ...");

        kb->prepare_compile();

        processor.add_component(new proc::compile_kb_t());
        processor.process(inputs);

        kb->finalize();

        print_console("Completed to compile knowledge-base.");
    }

    /* INFERENCE */
    if (config.mode == bin::EXE_MODE_INFERENCE)
    {
        std::vector<lf::input_t> parsed_inputs;
        proc::processor_t processor;
        bool flag_printing(false);

        print_console("Loading observations ...");

        processor.add_component(new proc::parse_obs_t(&parsed_inputs));
        processor.process(inputs);

        print_console("Completed to load observations.");
        print_console_fmt("    # of observations: %d", parsed_inputs.size());

        kb::knowledge_base_t::instance()->prepare_query();

        if (phillip->check_validity())
        if (kb::knowledge_base_t::instance()->is_valid_version())
        for (int i = 0; i < parsed_inputs.size(); ++i)
        {
            const lf::input_t &ipt = parsed_inputs.at(i);

            std::string obs_name = ipt.name;
            if (obs_name.rfind("::") != std::string::npos)
                obs_name = obs_name.substr(obs_name.rfind("::") + 2);

            if (phillip->is_target(obs_name) and
                not phillip->is_excluded(obs_name))
            {
                if (not flag_printing)
                {
                    phillip->write_header();
                    flag_printing = true;
                }

                print_console_fmt("Observation #%d: %s", i, ipt.name.c_str());
                kb::knowledge_base_t::instance()->clear_distance_cache();
                phillip->infer(ipt);


                auto sols = phillip->get_solutions();
                for (auto sol = sols.begin(); sol != sols.end(); ++sol)
                    sol->print_graph();
            }
        }

        if (flag_printing)
            phillip->write_footer();
    }
}


bool parse_options(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
{
    int opt;
    
    while ((opt = getopt(argc, argv, ACCEPTABLE_OPTIONS)) != -1)
    {
        std::string arg((optarg == NULL) ? "" : optarg);
        int ret = _interpret_option( opt, arg, phillip, config, inputs );
        
        if (not ret)
            throw phillip_exception_t(
            "Any error occured during parsing command line options:"
            + format("-%c %s", opt, arg.c_str()), true);
    }

    for (int i = optind; i < argc; i++)
        inputs->push_back(normalize_path(argv[i]));

    return true;
}


/** Load the setting file. */
bool _load_config_file(
    const char* filename, phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs )
{
    char line[2048];
    std::ifstream fin( filename );

    if (not fin)
        throw phillip_exception_t(format(
        "Cannot open setting file \"%s\"", filename));

    print_console_fmt("Loading setting file \"%s\"", filename);
    
    while( not fin.eof() )
    {
        fin.getline( line, 2048 );
        if( line[0] == '#' ) continue; // COMMENT

        std::string sline(line);
        auto spl = phil::split(sline, " \t", 1);
        
        if( spl.empty() ) continue;

        if( spl.at(0).at(0) == '-' )
        {
            int opt = static_cast<int>( spl.at(0).at(1) );
            std::string arg = (spl.size() <= 1) ? "" : strip(spl.at(1), "\n");
            int ret = _interpret_option( opt, arg, phillip, config, inputs );
            
            if (not ret)
                throw phillip_exception_t(
                "Any error occured during parsing command line options:"
                + std::string(line), true);
        }
        if (spl.at(0).at(0) != '-' and spl.size() == 1)
            inputs->push_back(normalize_path(spl.at(0)));
    }

    fin.close();
    return true;
}


bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
{
    switch(opt)
    {
        
    case 'c': // ---- SET COMPONENT
    {
        int idx( arg.find('=') );
        if (idx >= 0)
        {
            std::string type = arg.substr(0, idx);
            std::string key = arg.substr(idx + 1);
            if (type == "lhs")
            {
                config->lhs_key = key;
                return true;
            }
            if (type == "ilp")
            {
                config->ilp_key = key;
                return true;
            }
            if (type == "sol")
            {
                config->sol_key = key;
                return true;
            }
            if (type == "dist")
            {
                config->dist_key = key;
                return true;
            }
            if (type == "tab")
            {
                config->tab_key = key;
                return true;
            }
        }
        return false;
    }

    case 'e': // ---- SET NAME OF THE OBSERVATION TO EXCLUDE
    {
        config->excluded_obs_names.insert(arg);
        return true;
    }
    
    case 'f':
        phillip->set_flag(arg);
        return true;

    case 'h':
        config->mode = EXE_MODE_HELP;
        return true;
    
    case 'k': // ---- SET FILENAME OF KNOWLEDGE-BASE
    {
        config->kb_name = normalize_path(arg);
        return true;
    }

    case 'l': // ---- SET THE PATH OF PHILLIP CONFIGURE FILE
    {
        std::string path = normalize_path(arg);
        _load_config_file(path.c_str(), phillip, config, inputs);
        return true;
    }
    
    case 'm': // ---- SET MODE
    {
        if (config->mode != EXE_MODE_HELP)
        {
            if (arg == "inference")
                config->mode = EXE_MODE_INFERENCE;
            else if (arg == "compile_kb")
                config->mode = EXE_MODE_COMPILE_KB;
            else
                config->mode = EXE_MODE_UNDERSPECIFIED;
        }

        return (config->mode != EXE_MODE_UNDERSPECIFIED);
    }
        
    case 'o': // ---- SET NAME OF THE OBSERVATION TO SOLVE
    {
        config->target_obs_names.insert(arg);
        return true;
    }
        
    case 'p': // ---- SET PARAMETER
    {
        int idx(arg.find('='));
        
        if( idx != std::string::npos )
        {
            std::string key = arg.substr(0, idx);
            std::string val = arg.substr(idx + 1);
            if (startswith(key, "path"))
                val = normalize_path(val);
            phillip->set_param(key, val);
        }
        else
            phillip->set_param(arg, "");
        
        return true;
    }

    case 't': // ---- SET THREAD NUM
    {
        auto spl = split(arg, "=");
        if (spl.size() == 1)
        {
            phillip->set_param("kb_thread_num", spl[0]);
            phillip->set_param("gurobi_thread_num", spl[0]);
            return true;
        }
        else if (spl.size() == 2)
        {
            if (spl[0] == "kb")
            {
                phillip->set_param("kb_thread_num", spl[1]);
                return true;
            }
            else if (spl[0] == "grb")
            {
                phillip->set_param("gurobi_thread_num", spl[1]);
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    
    case 'v': // ---- SET VERBOSITY
    {
        int v(-1);
        int ret = _sscanf( arg.c_str(), "%d", &v );

        if( v >= 0 and v <= FULL_VERBOSE )
        {
            phillip->set_verbose(v);
            return true;
        }
        else
            return false;
    }

    case 'P':
        phillip->set_flag("get_pseudo_positive");
        return true;
        
    case 'T': // ---- SET TIMEOUT [SECOND]
    {                  
        int t;
        auto spl = split(arg, "=");
        if (spl.size() == 1)
        {
            _sscanf(arg.c_str(), "%d", &t);
            phillip->set_timeout_all(t);
            return true;
        }
        else if (spl.size() == 2)
        {
            if (spl[0] == "lhs" or spl[0] == "ilp" or spl[0] == "sol")
            {
                _sscanf(spl[1].c_str(), "%d", &t);
                if (spl[0] == "lhs") phillip->set_timeout_lhs(t);
                if (spl[0] == "ilp") phillip->set_timeout_ilp(t);
                if (spl[0] == "sol") phillip->set_timeout_sol(t);
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    
    case ':':
    case '?':
        return false;
    }
    
    return false;
}


bool preprocess(const execution_configure_t &config, phillip_main_t *phillip)
{
    if (config.mode == EXE_MODE_UNDERSPECIFIED)
        throw phillip_exception_t("Execution mode is underspecified.", true);

    float max_dist = phillip->param_float("kb_max_distance", -1.0);
    int thread_num = phillip->param_int("kb_thread_num", 1);
    bool disable_stop_word = phillip->flag("disable_stop_word");
    std::string dist_key = config.dist_key.empty() ? "basic" : config.dist_key;
    std::string tab_key = config.tab_key.empty() ? "null" : config.tab_key;

    for (auto n : config.target_obs_names)
        phillip->add_target(n);
    for (auto n : config.excluded_obs_names)
        phillip->add_exclusion(n);

    lhs_enumerator_t *lhs =
        lhs_enumerator_library_t::instance()->
        generate(config.lhs_key, phillip);
    ilp_converter_t *ilp =
        ilp_converter_library_t::instance()->
        generate(config.ilp_key, phillip);
    ilp_solver_t *sol =
        ilp_solver_library_t::instance()->
        generate(config.sol_key, phillip);

    kb::knowledge_base_t::setup(
        config.kb_name, max_dist, thread_num, disable_stop_word);
    kb::knowledge_base_t::instance()->set_distance_provider(dist_key);
    kb::knowledge_base_t::instance()->set_category_table(tab_key);

    switch (config.mode)
    {
    case EXE_MODE_INFERENCE:
        if (lhs != NULL) phillip->set_lhs_enumerator(lhs);
        if (ilp != NULL) phillip->set_ilp_convertor(ilp);
        if (sol != NULL) phillip->set_ilp_solver(sol);
        return true;
    case EXE_MODE_COMPILE_KB:
        return true;
    default:
        return false;
    }
}


void print_usage()
{
    /** String for usage printing. */
    const std::vector<std::string> USAGE = {
        "Usage:",
        "  $phil -m [MODE] [OPTIONS] [INPUTS]",
        "",
        "  Mode:",
        "    -m inference : Inference mode.",
        "    -m compile_kb : Compiling knowledge-base mode.",
        "",
        "  Common Options:",
        "    -l <NAME> : Load a config-file.",
        "    -p <NAME>=<VALUE> : set a parameter.",
        "    -f <NAME> : Set a flag.",
        "    -t <INT> : Set the number of threads for parallelization.",
        "    -v <INT> : Set verbosity (0 ~ 5).",
        "    -h : Print this usage.",
        "",
        "  Options in inference-mode:",
        "    -c lhs=<NAME> : Set a component for making latent hypotheses sets.",
        "    -c ilp=<NAME> : Set a component for making ILP problems.",
        "    -c sol=<NAME> : Set a component for making solution hypotheses.",
        "    -k <NAME> : Set the prefix of the path of the compiled knowledge base.",
        "    -o <NAME> : Solve only the observation of corresponding name.",
        "    -e <NAME> : Exclude the observation of corresponding name from inference.",
        "    -T <INT>  : Set timeout of the whole inference in seconds.",
        "    -T lhs=<INT> : Set timeout of the creation of latent hypotheses sets in seconds.",
        "    -T ilp=<INT> : Set timeout of the conversion into ILP problem in seconds.",
        "    -T sol=<INT> : Set timeout of the optimization of ILP problem in seconds.",
        "",
        "  Options in compile_kb mode:",
        "    -c dist=<NAME> : Set a component to define relatedness between predicates.",
        "    -c tab=<NAME> : Set a component for making category-table.",
        "    -k <NAME> : Set the prefix of the path of the compiled knowledge base.",
        "",
        "  Wiki: https://github.com/kazeto/phillip/wiki"};

    for (auto s : USAGE)
        print_console(s);
}


}

}
