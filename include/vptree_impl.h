#ifndef KMCOMP_VPTREE_IMPL_H
#define KMCOMP_VPTREE_IMPL_H

#include <vptree.h>

namespace kmcomp
{
    template <class T>
    VPTree<T>::VPTree(VPTree* parent, const std::vector<T>& vertices, const DistanceFunction<T>& distFunc)
    {
        this->parent = parent;
        this->distFunc = distFunc;
        
        if(vertices.size() == 0)
            throw std::runtime_error("ERROR: Attempted to initialize VPTree (or one of its nodes) with no elements");

        if(vertices.size() == 1)
        {
            pivot = vertices[0];
            return;
        }

        //Select a random vertex as pivot
        std::size_t pivotIndex = RNG::rand_uint32_t(0, vertices.size());
        pivot = vertices[pivotIndex];

        //Divide space by two
        std::vector<T> leftVertices, rightVertices;
        leftVertices.reserve(vertices.size()/2);
        rightVertices.reserve(vertices.size()/2);

        //Distance scope
        {
            std::vector<double> distances;
            distances.reserve(vertices.size() - 1);

            //Before pivot
            for(std::size_t i = 0; i < pivotIndex; ++i)
            {
                double d = this->distFunc(pivot, vertices[i]);
                distances.push_back(d);
            }

            //After pivot
            for(std::size_t i = pivotIndex+1; i < vertices.size(); ++i)
            {
                double d = this->distFunc(pivot, vertices[i]);
                distances.push_back(d);
            }

            //nlog(n) median but should be quick as it needs to sort small lists which size decrease
            threshold = nlogn_median(distances);
            //threshold = quickselect_median(distances);

            //Before pivot
            for(std::size_t i = 0; i < pivotIndex; ++i)
            {
                if(distances[i] < threshold)
                    leftVertices.push_back(vertices[i]);
                else
                    rightVertices.push_back(vertices[i]);
            }

            //After pivot
            for(std::size_t i = pivotIndex+1; i < vertices.size(); ++i)
            {
                if(distances[i-1] < threshold)
                    leftVertices.push_back(vertices[i]);
                else
                    rightVertices.push_back(vertices[i]);
            }
        }

        if(leftVertices.size() > 0)
            left = new VPTree(this, leftVertices, distFunc);

        if(rightVertices.size() > 0)
            right = new VPTree(this, rightVertices, distFunc);
    }

    template <class T>
    VPTree<T>::VPTree(const std::vector<T>& vertices, const DistanceFunction<T>& distFunc) : VPTree(nullptr, vertices, distFunc) {}

    template <class T>
    VPTree<T>::~VPTree()
    {
        if(left != nullptr)
            delete left;

        if(right != nullptr)
            delete right;

        left = right = nullptr;
    }

    template <class T>
    void VPTree<T>::get_unvisited_nearest_neighbor(T query, const std::vector<bool>& alreadyAdded, double* tau, T* currentResult)
    {
        if(query < 0)
            throw std::runtime_error("ERROR: Can't query invalid vertex");

        //Check if distance already has been computed
        double distance = distFunc(pivot, query);

        if(!alreadyAdded[pivot] && distance < *tau) //See if it prevents algorithm from converging (since tau is not updated), it shouldn't as there are no cycles
        {
            *tau = distance;
            *currentResult = pivot;
        }

        if(distance < threshold)
        {
            if(left != nullptr && !left->skip && (distance - *tau) <= threshold)
                left->get_unvisited_nearest_neighbor(query, alreadyAdded, tau, currentResult);

            if(right != nullptr && !right->skip && (distance + *tau) >= threshold)
                right->get_unvisited_nearest_neighbor(query, alreadyAdded, tau, currentResult);
        }
        else
        {
            if(right != nullptr && !right->skip && (distance + *tau) >= threshold)
                right->get_unvisited_nearest_neighbor(query, alreadyAdded, tau, currentResult);

            if(left != nullptr && !left->skip && (distance - *tau) <= threshold)
                left->get_unvisited_nearest_neighbor(query, alreadyAdded, tau, currentResult);
        }
    }

    template <class T>
    void VPTree<T>::update(VPTree<T>* node, const std::vector<bool>& alreadyAdded)
    {
        while(node != nullptr)
        {
            node->skip = (node->left == nullptr || node->left->skip) && (node->right == nullptr || node->right->skip) && alreadyAdded[node->pivot];
            
            //Stop property propagation if not masked
            if(!node->skip)
                return;

            node = node->parent;
        }
    }

    //Fail if vector is not big enough (should be size = n)
    template <class T>
    void VPTree<T>::map_nodes(VPTree<T>* node, std::vector<VPTree<T>*>& out_vector)
    {
        if(node == nullptr)
            return;

        out_vector[node->pivot] = node;

        map_nodes(node->left, out_vector);
        map_nodes(node->right, out_vector);
    }
};

#endif
