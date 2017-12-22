#include "ConnectionIntrospector.hpp"

// Add helpers, with an ifdef on using CORBA for actually including (and testing) CORBA stuff
// Should convert, using dynamic_cast, to various types... But if I remember correctly I don't have
// access to such types (CORBA specifically) as it's a separate library which doesn't get linked
// here...

// Should make a "node" ABC class, and inherit with port_node, buffer_node.
// Connections are then a list of ordered pairs (could consider only one-way connections for now).
// Or: node -> { {node, lvl}, {node, lvl} , ... } so that, if { node -> node } exists, lvl is changed to
// the lowest so far... THIS WON'T WORK CONSIDERING THAT YOU CAN GO OUT -> IN -> OUT...

#include "../base/ChannelElement.hpp"

namespace RTT {
namespace internal {

// Derive a class from Multi-connection-stuff, just to access multi-ports... which are private!
class MultiOutputsChannelAccessor : virtual public RTT::base::MultipleOutputsChannelElementBase {
public:
    MultiOutputsChannelAccessor(const RTT::base::MultipleOutputsChannelElementBase& parent)
        : RTT::base::MultipleOutputsChannelElementBase(parent){
        std::cout << "# outputs: " << outputs.size() << std::endl;
    }
    Outputs getMultiOutputs() {
        RTT::os::SharedMutexLock lock(outputs_lock);
        return outputs;
    }
};
class MultiInputsChannelAccessor : virtual public RTT::base::MultipleInputsChannelElementBase {
public:
    MultiInputsChannelAccessor(const RTT::base::MultipleInputsChannelElementBase& parent)
        : RTT::base::MultipleInputsChannelElementBase(parent){
        std::cout << "# inputs: " << inputs.size() << std::endl;
    }
    Inputs getMultiInputs() {
        RTT::os::SharedMutexLock lock(inputs_lock);
        return inputs;
    }
};
class MultiInputsMultiOutputsChannelAccessor : virtual public RTT::base::MultipleInputsMultipleOutputsChannelElementBase, virtual public MultiOutputsChannelAccessor, virtual public MultiInputsChannelAccessor {
public:
    MultiInputsMultiOutputsChannelAccessor(const RTT::base::MultipleInputsMultipleOutputsChannelElementBase& parent)
        : MultiOutputsChannelAccessor(parent), MultiInputsChannelAccessor(parent) { }
    using RTT::base::MultipleInputsMultipleOutputsChannelElementBase::connected;
    using RTT::base::MultipleInputsMultipleOutputsChannelElementBase::disconnect;
};

void printMultiChannel(const ConnectionManager::ChannelDescriptor& cd) {
    base::ChannelElementBase::shared_ptr channel_elem_ptr = cd.get<1>();
    base::ChannelElementBase::shared_ptr out_ep = channel_elem_ptr;
    base::ChannelElementBase::shared_ptr in_ep = channel_elem_ptr;
    int out_ops = 0, in_ops = 0;
    while (out_ep->getOutput()) {
        out_ep = out_ep->getOutput();
        out_ops++;
        if (out_ep->getPort()) {
            std::cout << "** out iter #" << out_ops << " : port "
                      << ConnectionIntrospector::PortQualifier(
                             out_ep->getPort()) << std::endl;
        }
    }
    std::cout << "** ** total out iters #" << in_ops << " ** **" << std::endl;
    while (in_ep->getInput()) {
        in_ep = in_ep->getInput();
        in_ops++;
        if (in_ep->getPort()) {
            std::cout << "** in iter #" << in_ops << " : port "
                      << ConnectionIntrospector::PortQualifier(in_ep->getPort())
                      << std::endl;
        }
    }
    std::cout << "** ** total in iters #" << in_ops << " ** **" << std::endl;

    if (dynamic_cast<RTT::base::MultipleInputsMultipleOutputsChannelElementBase*>(channel_elem_ptr.get())) {
        MultiInputsMultiOutputsChannelAccessor ca(*dynamic_cast<RTT::base::MultipleInputsMultipleOutputsChannelElementBase*>(channel_elem_ptr.get()));
    } else if (dynamic_cast<RTT::base::MultipleInputsChannelElementBase*>(channel_elem_ptr.get())) {
        MultiInputsChannelAccessor ca(*dynamic_cast<RTT::base::MultipleInputsChannelElementBase*>(channel_elem_ptr.get()));
    } else if (dynamic_cast<RTT::base::MultipleOutputsChannelElementBase*>(channel_elem_ptr.get())) {
        MultiOutputsChannelAccessor ca(*dynamic_cast<RTT::base::MultipleOutputsChannelElementBase*>(channel_elem_ptr.get()));
    } else {
        std::cout << " -- non-multi channel --" << std::endl;
    }
}

}  // namespace internal
}  // namespace RTT
