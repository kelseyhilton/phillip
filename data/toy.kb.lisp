(B (name banana_is_food)
   (=> (banana-nn x) (food-nn x)))
(B (name hungry_eat)
   (=> (^ (hungry-jj e1) (nsubj e1 x))
       (^ (eat-vb e2) (nsubj e2 x))))
(B (name eat_food)
   (=> (^ (eat-vb e) (dobj e x))
       (food-nn x :opt)))
(B (name chimp_eats_banana)
   (=> (chimp-nn x1)
       (^ (eat-vb e) (banana-nn x2) (nsubj e x1) (dobj e x2))))

(B (unipp (nsubj * .)))
(B (unipp (dobj * .)))

(B (xor (nsubj e x) (dobj e x)))

