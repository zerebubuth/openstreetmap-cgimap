

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <sstream>





/* OSMChange_Tracking
 *
 * - Keeps track of old_id->new_id mappings in case of new objects
 *   as well as version number changes in case of modify and delete operations
 */

OSMChange_Tracking::OSMChange_Tracking() {}


