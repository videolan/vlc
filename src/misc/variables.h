typedef struct callback_entry_t callback_entry_t;

/**
 * The structure describing a variable.
 * \note vlc_value_t is the common union for variable values
 */
struct variable_t
{
    /** The variable's exported value */
    vlc_value_t  val;

    char *       psz_name; /**< The variable unique name */
    uint32_t     i_hash;   /**< (almost) unique hashed value */
    int          i_type;   /**< The type of the variable */

    /** The variable display name, mainly for use by the interfaces */
    char *       psz_text;

    /** A pointer to a comparison function */
    int      ( * pf_cmp ) ( vlc_value_t, vlc_value_t );
    /** A pointer to a duplication function */
    void     ( * pf_dup ) ( vlc_value_t * );
    /** A pointer to a deallocation function */
    void     ( * pf_free ) ( vlc_value_t * );

    /** Creation count: we only destroy the variable if it reaches 0 */
    int          i_usage;

    /** If the variable has min/max/step values */
    vlc_value_t  min, max, step;

    /** Index of the default choice, if the variable is to be chosen in
     * a list */
    int          i_default;
    /** List of choices */
    vlc_list_t   choices;
    /** List of friendly names for the choices */
    vlc_list_t   choices_text;

    /** Set to TRUE if the variable is in a callback */
    vlc_bool_t   b_incallback;

    /** Number of registered callbacks */
    int                i_entries;
    /** Array of registered callbacks */
    callback_entry_t * p_entries;
};

