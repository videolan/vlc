#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "RoundRobinLogic.hpp"

#ifdef ADAPTIVE_DEBUGGING_LOGIC

#include "Representationselectors.hpp"
#include "../playlist/BaseAdaptationSet.h"

using namespace adaptive::logic;
using namespace adaptive::playlist;

RoundRobinLogic::RoundRobinLogic    (vlc_object_t *obj) :
                           AbstractAdaptationLogic      (obj)
{
    index = 0;
    call = 0;
}

BaseRepresentation *RoundRobinLogic::getNextRepresentation(BaseAdaptationSet *adaptSet,
                                                           BaseRepresentation *)
{
    if(adaptSet->getRepresentations().size() == 0)
        return nullptr;
    if((++call % QUANTUM) == 0)
        index++;
    if(index >= adaptSet->getRepresentations().size())
        index = 0;
    return adaptSet->getRepresentations().at(index);
}

#endif
