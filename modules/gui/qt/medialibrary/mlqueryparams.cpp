#include "mlqueryparams.hpp"

vlc_ml_query_params_t MLQueryParams::toCQueryParams() const
{
    vlc_ml_query_params_t params;
    params.psz_pattern = searchPatternUtf8.isNull()
                       ? nullptr
                       : searchPatternUtf8.constData();
    params.i_nbResults = nbResults;
    params.i_offset = offset;
    params.i_sort = sort;
    params.b_desc = desc;
    return params;
}
