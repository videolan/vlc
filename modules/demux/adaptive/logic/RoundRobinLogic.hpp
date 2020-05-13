#ifndef ROUNDROBINLOGIC_HPP
#define ROUNDROBINLOGIC_HPP

#include "../tools/Debug.hpp"

#ifdef ADAPTIVE_DEBUGGING_LOGIC
#include "AbstractAdaptationLogic.h"

namespace adaptive
{
    namespace logic
    {
        class RoundRobinLogic : public AbstractAdaptationLogic
        {
            public:
                RoundRobinLogic(vlc_object_t *);

                virtual BaseRepresentation* getNextRepresentation(BaseAdaptationSet *,
                                                                  BaseRepresentation *);
                static const unsigned QUANTUM = 2;
            private:
                size_t index;
                uint64_t call;
        };
    }
}
#endif

#endif // ROUNDROBINLOGIC_HPP
