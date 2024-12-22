#pragma once

/// \file
/// \brief Contains standard macro solution for creating arbitrary pragmas from macros.

/// \brief Performs stringizing on all the inputs and puts it into pragma.
/// \warning Stringizing does not expands nested macros as per standards of C preprocessing.
#define KAN_MAKE_PRAGMA(...) _Pragma (#__VA_ARGS__)
