#include "Ele.h"Ele::Ele(const Ele& e){	if (e.type == intermediate_node) {		childValueLowBound = e.child_value_low_bound;		child = e.child;	} else if (e.type == leaf) {		key = e.key;		data = e.data;	}}inline Ele& Ele::operator=(const Ele& e){	if (e.type == intermediate_node) {		childValueLowBound = e.child_value_low_bound;		child = e.child;	} else if (e.type == leaf) {		key = e.key;		data = e.data;	}	return *this;}inline Ele::~Ele(){	if (type == intermediate_node) {		childValueLowBound.~KeyType();		child.~shared_ptr();	} else if (type == leaf) {		key.~KeyType();		data.~DataType();	}}