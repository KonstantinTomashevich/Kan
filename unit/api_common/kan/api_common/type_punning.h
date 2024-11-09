#pragma once

/// \file
/// \brief Contains utility macro for type punning -- representing instances of one type as another similar type.
/// \details We need to use union in order to make it readable for GCC.

#define KAN_PUN_TYPE(FROM, TO, VARIABLE)                                                                               \
    (((union {                                                                                                         \
         FROM source;                                                                                                  \
         TO target;                                                                                                    \
     } *) &(VARIABLE))                                                                                                 \
         ->target)

#define KAN_PUN_TYPE_RECEIVE_AND_RETURN(FROM, TO, VARIABLE)                                                            \
    union                                                                                                              \
    {                                                                                                                  \
        FROM source;                                                                                                   \
        TO target;                                                                                                     \
    } pun_type_data;                                                                                                   \
    pun_type_data.source = (VARIABLE);                                                                                 \
    return pun_type_data.target
