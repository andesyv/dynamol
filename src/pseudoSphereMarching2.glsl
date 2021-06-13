endIndex = 0;
minIndex = 0;
sortedIndex = 0;
t = 0.0;
far = 1.0;
while (t < far)
    // Increment startIndex if already past intersecting sphere
    while (far(minIndex) < t)
        minIndex++
    
    surfaceIterations = 0
    minDistance = MAX
    for (startIndex = minIndex; surfaceIterations < MAX_ITERATIONS; surfaceIterations++, startIndex++)
        // Increment endIndex
        while (near(endIndex) < far(startIndex) && endIndex < entryCount - 1)
            endIndex++
            // Sort up to endIndex (incremental selective sort)
            // Only sort if we haven't already sorted up to that index
            if (sortedIndex < endIndex)
                current = endIndex + 1;
                minimalIndex = current;
                // Find smallest in [endIndex, end]
                for (; current < entryCount; current++)
                    if (near(current) < near(minimalIndex))
                        minimalIndex = current;
                // Swap
                if (current != minimalIndex)
                    swap(current, minimalIndex);
                sortedIndex++
        
        // Find distance to current surface
        for (i = startIndex; i < endIndex-1; i++)
            // All spheres behind t won't contribute to closest surface
            surfaceDist += dist(i);
        
        // We found the surface. Break
        if (surfaceDist ~= 0)
            return t;
        
        if (surfaceDist < minDistance)
            minDistance = surfaceDist

    // Finally increment t with closest surface (as to not overstep)
    t += minDistance;

    
