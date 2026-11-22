<script lang="ts">
  // Corner flourish of the stats panel. Ten small vector pieces from the design
  // (Figma node 18:12265), each rotated a quarter turn, placed with the
  // positions exported from the design file. `mirrored` gives the right corner.
  import piece1 from '../assets/design/ornament-1.svg'
  import piece2 from '../assets/design/ornament-2.svg'
  import piece3 from '../assets/design/ornament-3.svg'
  import piece4 from '../assets/design/ornament-4.svg'
  import piece5 from '../assets/design/ornament-5.svg'
  import piece6 from '../assets/design/ornament-6.svg'
  import piece7 from '../assets/design/ornament-7.svg'
  import piece8 from '../assets/design/ornament-8.svg'
  import piece9 from '../assets/design/ornament-9.svg'
  import piece10 from '../assets/design/ornament-10.svg'

  let { mirrored = false }: { mirrored?: boolean } = $props()
</script>

<div class="ornament" class:mirrored aria-hidden="true">
  <img alt="" src={piece1} style="left: 20.94px; top: -5.50px; width: 8.72px; height: 19.95px;" />
  <img alt="" src={piece2} style="left: 14.78px; top: 7.99px; width: 11.75px; height: 5.31px;" />
  <img alt="" src={piece3} style="left: 8.79px; top: 14.89px; width: 10.47px; height: 6.74px;" />
  <img alt="" src={piece4} style="left: 8.05px; top: -2.18px; width: 6.16px; height: 13.09px;" />
  <img alt="" src={piece5} style="left: 14.74px; top: 7.67px; width: 3.84px; height: 6.18px;" />
  <img alt="" src={piece6} style="left: 7.57px; top: 12.27px; width: 3.49px; height: 8.50px;" />
  <img alt="" src={piece7} style="left: 6.32px; top: 7.82px; width: 8.96px; height: 3.79px;" />
  <img alt="" src={piece8} style="left: -1.61px; top: 1.61px; width: 12.44px; height: 9.22px;" />
  <img alt="" src={piece9} style="left: -1.96px; top: 13.00px; width: 12.10px; height: 6.10px;" />
  <img alt="" src={piece10} style="left: -2.53px; top: 21.18px; width: 16.63px; height: 11.49px;" />
</div>

<style>
  .ornament {
    position: absolute;
    top: -1.5px;
    left: -0.64px;
    width: 35.27px;
    height: 35.26px;
    pointer-events: none;
  }
  .ornament.mirrored {
    left: auto;
    right: -0.64px;
    transform: scaleX(-1);
  }
  img {
    position: absolute;
    display: block;
    transform: rotate(90deg);
  }
</style>
