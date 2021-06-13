- Algorithm:
   - endIndex++
   - Selection sort up to current index
   - If start->end intersecting:
     - Sphere march
       - Sum up contributions
       - t += sum
     - startIndex++

 - New algorithm:
   - startIndex = 0
   - while no surface been found:
     - If endIndex is past intersecting spheres (if far(startIndex) < near(endIndex) )
       - Sphere march
         - Sum up contributions from startIndex to endIndex - 1
         - t += sum
         - if surface found, end
       - startIndex++
     - else:
       - endIndex++
       - Selection sort up to end index
     

 - New new algorithm:
   - startIndex = endIndex = 0
   - While no surface been found:
     - while t < far:
       - while far(startIndex) < t
         - startIndex++
       - while near(endIndex) < far(startIndex):
         - endIndex++
         - Selection sort up to endIndex
           - find smallest in [endIndex, end]
           - swap smallest with endIndex
         - Update far for new range
           - far = far(endIndex)

       - dist = sum(startIndex, endIndex);
       - if dist close to zero we found surface. end.
       - t += dist;
     - 


