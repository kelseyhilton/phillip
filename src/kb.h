#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <ctime>

#include "./define.h"
#include "./logical_function.h"


namespace phil
{


namespace pg { class proof_graph_t; }


namespace kb
{

static const axiom_id_t INVALID_AXIOM_ID = -1;
static const argument_set_id_t INVALID_ARGUMENT_SET_ID = 0;
static const arity_id_t INVALID_ARITY_ID = 0;


enum unification_postpone_argument_type_e
{
    UNI_PP_INDISPENSABLE,           /// Is expressed as '*'.
    UNI_PP_INDISPENSABLE_PARTIALLY, /// Is expressed as '+'.
    UNI_PP_DISPENSABLE,             /// Is expressed as '.'.
};


enum version_e
{
    KB_VERSION_UNDERSPECIFIED,
    KB_VERSION_1, KB_VERSION_2, KB_VERSION_3, KB_VERSION_4, KB_VERSION_5,
    KB_VERSION_6,
    NUM_OF_KB_VERSION_TYPES
};


/** A virtual class to define distance between predicates
 *  on creation of reachable-matrix. */
class distance_provider_t
{
public:
    virtual ~distance_provider_t() {}
    virtual float operator() (const lf::axiom_t &ax) const = 0;

    virtual std::string repr() const = 0;
};


/** A virtual class of table which has semantic gaps between predicates. */
class category_table_t
{
public:
    enum table_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    category_table_t() : m_state(STATE_NULL) {}
    virtual ~category_table_t() {}

    virtual void prepare_compile(const knowledge_base_t *base) = 0;
    virtual void prepare_query(const knowledge_base_t *base) = 0;

    /** Updates the elements corresponding to given axiom in the table.
     *  This method is called by knowledge_base_t::insert_implication. */
    virtual void add(const lf::logical_function_t &ax) = 0;

    /** Returns the semantic gap between p1 & p2, which is a positive value.
    *  If p1 cannot be p2, returns -1. */
    virtual float get(const arity_t &p1, const arity_t &p2) const = 0;

    virtual void finalize() = 0;

protected:
    table_state_e m_state;
    std::string m_prefix;
};


class unification_postponement_t
{
public:
    unification_postponement_t() {}
    unification_postponement_t(
        const std::string &arity, const std::vector<char> &args,
        int num_for_partial_indispensability);

    bool do_postpone(const pg::proof_graph_t*, index_t n1, index_t n2) const;
    inline bool empty() const { return m_args.empty(); }

private:
    std::string m_arity;
    std::vector<char> m_args;
    int m_num_for_partial_indispensability;
};


/** A class of knowledge-base. */
class knowledge_base_t
{
public:
    static knowledge_base_t* instance();
    static void setup(
        std::string filename, float max_distance,
        int thread_num_for_rm, bool do_disable_stop_word);
    static inline float get_max_distance();

    ~knowledge_base_t();

    /** Initializes knowledge base and
     *  prepares for compiling knowledge base. */
    void prepare_compile();

    /** Prepares for reading knowledge base. */
    void prepare_query();

    /** Call this method on end of compiling or reading knowledge base. */
    void finalize();

    axiom_id_t insert_implication(
        const lf::logical_function_t &f, const std::string &name);
    axiom_id_t insert_inconsistency(
        const lf::logical_function_t &f, const std::string &name);
    axiom_id_t insert_unification_postponement(
        const lf::logical_function_t &f, const std::string &name);
    void insert_argument_set(const lf::logical_function_t &f);

    inline lf::axiom_t get_axiom(axiom_id_t id) const;
    inline std::list<axiom_id_t> search_axioms_with_rhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_axioms_with_lhs(const std::string &arity) const;
    inline std::list<axiom_id_t> search_inconsistencies(const std::string &arity) const;
    inline arity_id_t search_arity_id(const std::string &arity) const;
    hash_set<axiom_id_t> search_axiom_group(axiom_id_t id) const;
    unification_postponement_t get_unification_postponement(const std::string &arity) const;
    argument_set_id_t search_argument_set_id(const std::string &arity, int term_idx) const;
    void search_queries(arity_id_t arity, std::list<search_query_t> *out) const;
    void search_axioms_with_query(
        const search_query_t &query,
        std::list<std::pair<axiom_id_t, bool> > *out) const;

    void set_distance_provider(const std::string &key);
    void set_category_table(const std::string &key);

    /** Returns ditance between arity1 and arity2
     *  in a reachable-matrix in the current knowledge-base.
     *  If these arities are not reachable, then return -1. */
    float get_distance(
        const std::string &arity1, const std::string &arity2) const;

    /** Returns distance between arity1 and arity2 with distance-provider. */
    inline float get_distance(const lf::axiom_t &axiom) const;
    inline float get_distance(axiom_id_t id) const;

    inline version_e version() const;
    inline bool is_valid_version() const;
    inline const std::string& filename() const;
    inline int num_of_axioms() const;

    inline void clear_distance_cache();

private:
    class axioms_database_t
    {
    public:
        axioms_database_t(const std::string &filename);
        ~axioms_database_t();
        void prepare_compile();
        void prepare_query();
        void finalize();
        void put(const std::string &name, const lf::logical_function_t &func);
        lf::axiom_t get(axiom_id_t id) const;
        inline bool is_writable() const;
        inline bool is_readable() const;
        inline int num_axioms() const { return m_num_compiled_axioms; }

    private:
        typedef unsigned long long axiom_pos_t;
        typedef unsigned long axiom_size_t;

        inline std::string get_name_of_unnamed_axiom();

        static std::mutex ms_mutex;
        std::string m_filename;
        std::ofstream *m_fo_idx, *m_fo_dat;
        std::ifstream *m_fi_idx, *m_fi_dat;
        int m_num_compiled_axioms, m_num_unnamed_axioms;
        axiom_pos_t m_writing_pos;
    };

    /** A class of reachable-matrix for all predicate pairs. */
    class reachable_matrix_t
    {
    public:
        reachable_matrix_t(const std::string &filename);
        ~reachable_matrix_t();
        void prepare_compile();
        void prepare_query();
        void finalize();

        void put(size_t idx1, const hash_map<size_t, float> &dist);
        float get(size_t idx1, size_t idx2) const;
        hash_set<float> get(size_t idx) const;

        inline bool is_writable() const;
        inline bool is_readable() const;

    private:
        typedef unsigned long long pos_t;
        static std::mutex ms_mutex;
        std::string   m_filename;
        std::ofstream *m_fout;
        std::ifstream *m_fin;
        hash_map<size_t, pos_t> m_map_idx_to_pos;
    };

    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(const std::string &filename);

    void write_config() const;
    void read_config();

    void insert_arity(const std::string &arity);

    /** Outputs m_group_to_axioms to m_cdb_axiom_group. */
    void insert_axiom_group_to_cdb();
    void insert_argument_set_to_cdb();

    void set_stop_words();
    void create_query_map();
    void create_reachable_matrix();
    
    void _create_reachable_matrix_direct(
        const hash_set<std::string> &arities,
        const hash_set<arity_id_t> &ignored,
        hash_map<arity_id_t, hash_map<arity_id_t, float> > *out_lhs,
        hash_map<arity_id_t, hash_map<arity_id_t, float> > *out_rhs,
        std::set<std::pair<arity_id_t, arity_id_t> > *out_para);
    void _create_reachable_matrix_indirect(
        arity_id_t target,
        const hash_map<arity_id_t, hash_map<arity_id_t, float> > &base_lhs,
        const hash_map<arity_id_t, hash_map<arity_id_t, float> > &base_rhs,
        const std::set<std::pair<arity_id_t, arity_id_t> > &base_para,
        hash_map<arity_id_t, float> *out) const;

    void extend_inconsistency();
    void _enumerate_deducible_literals(
        const literal_t &target, hash_set<literal_t> *out) const;

    /** Returns axioms corresponding with given query.
     *  @param dat A database of cdb to seach axiom.
     *  @param tmp A map of temporal axioms related with dat. */
    std::list<axiom_id_t> search_id_list(
        const std::string &query, const cdb_data_t *dat) const;

    static std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t> > ms_instance;
    static std::string ms_filename;
    static float ms_max_distance;
    static int ms_thread_num_for_rm;
    static bool ms_do_disable_stop_word;
    static std::mutex ms_mutex_for_cache;
    static std::mutex ms_mutex_for_rm;

    kb_state_e m_state;
    std::string m_filename;
    version_e m_version;

    cdb_data_t m_cdb_name, m_cdb_rhs, m_cdb_lhs;
    cdb_data_t m_cdb_inc_pred, m_cdb_axiom_group, m_cdb_uni_pp, m_cdb_arg_set;
    cdb_data_t m_cdb_arity_to_queries, m_cdb_query_to_ids;
    cdb_data_t m_cdb_rm_idx;
    axioms_database_t m_axioms;
    reachable_matrix_t m_rm;

    hash_map<size_t, hash_map<size_t, float> > m_partial_reachable_matrix;

    /** All arities in this knowledge-base.
     *  This variable is used on constructing a reachable-matrix. */
    hash_set<std::string> m_arity_set;

    /** A set of arities of stop-words.
     *  These arities are ignored in constructing a reachable-matrix. */
    hash_set<std::string> m_stop_words;

    std::list<hash_set<std::string> > m_argument_sets;

    hash_map<std::string, hash_set<axiom_id_t> >
        m_name_to_axioms, m_lhs_to_axioms, m_rhs_to_axioms,
        m_inc_to_axioms, m_group_to_axioms, m_arity_to_postponement;

    /** Function object to provide distance between predicates. */
    struct
    {
        distance_provider_t *instance;
        std::string key;
    } m_distance_provider;

    struct
    {
        category_table_t *instance;
        std::string key;
    } m_category_table;

    bool m_do_create_local_reachability_matrix;
    
    mutable hash_map<size_t, hash_map<size_t, float> > m_cache_distance;
};


/** The namespace for super-classes of distance_provider_t. */
namespace dist
{

class basic_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator() (const lf::axiom_t&) const;
    virtual std::string repr() const { return "Basic"; };
};


class cost_based_distance_provider_t : public distance_provider_t
{
public:
    virtual float operator()(const lf::axiom_t&) const;
    virtual std::string repr() const { return "CostBased"; }
};

}


/** The namespace for super-classes of category_table_t. */
namespace ct
{

class basic_category_table_t : public category_table_t
{
public:
    virtual void prepare_compile(const knowledge_base_t*) override;
    virtual void add(const lf::logical_function_t &ax) override;

    virtual void prepare_query(const knowledge_base_t*) override;
    virtual float get(const arity_t &a1, const arity_t &a2) const override;

    virtual void finalize() override;

protected:
    void write(const std::string &filename) const;
    void read(const std::string &filename);

    std::string filename() const { return m_prefix + ".category.dat"; }

    hash_map<arity_t, hash_map<arity_t, float> > m_table_for_compile;
    hash_map<arity_id_t, hash_map<arity_id_t, float> > m_table_for_query;
};

}


void query_to_binary(const search_query_t &q, std::vector<char> *bin);
size_t binary_to_query(const char *bin, search_query_t *out);

}

}

#include "./kb.inline.h"
