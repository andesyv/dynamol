endIndex = 0;
for (startIndex = 0; startIndex < entryCount; startIndex++)
    // Increment endIndex
    while (near(endIndex) < far(startIndex))
        endIndex++
        // Sort up to endIndex (incremental selective sort)
        current = endIndex + 1;
        minimalIndex = current;
        // Find smallest in [endIndex, end]
        for (; current < entryCount; current++)
            if (near(current) < near(minimalIndex))
                minimalIndex = current;
        // Swap
        if (current != minimalIndex)
            swap(current, minimalIndex);

    // When we've found our overlapping spheres end, do the spheremarch
    for (t = 0.0; t < 1.0;)
        for (i = startIndex; i < endIndex-1; i++)
            // All spheres behind t won't contribute to closest surface
            if (far(i) < t)
                continue;
            surfaceDist += dist(i);
        
        // We found the surface. Break
        if (surfaceDist ~= 0)
            return t;
        else if (surfaceDist < 0)
            break;
        
        t += surfaceDist;

    
