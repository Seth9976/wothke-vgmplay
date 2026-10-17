let songs = [
	"Computers/IBM_PC/Doom_(PC)/02 At Doom\'s Gate.vgz;2",// boost volume <<2 for this one (hack for songs that for some reason don't play loud enough)
	"Computers/IBM_PC/Doom_(PC)/05 Kitchen Ace (and Taking Names).vgz;2",
	"Arcade/Taito/Bubble_Bobble_(Arcade)/01 Introduction ~ Main Theme.vgz;1",
	"Arcade/SegaSys/Out_Run_(Arcade)/01 Magical Sound Shower.vgz;1",
	"Arcade/Capcom/1944_-_The_Loop_Master_(CP_System_II)/01 Opening.vgz;1",
	"Sonyc_MS/01_Title.vgm",
	"Arcade/Taito/Twin_Cobra_(Toaplan_1)/02 Challenger (Opening Theme).vgz;2",
	"Arcade/Taito/Tiger-Heli_(Arcade)/02 First Mission.vgz;2",
	"Arcade/Konami/Yie_Ar_Kung-Fu_(Arcade)/02 Main BGM 1.vgz;2",
	"Arcade/Final_Lap_(Namco_System_2)/03 BGM A.vgz;2",
	"Arcade/Capcom/Ghouls%27n_Ghosts_(CP_System)/03 Stage 1 - The Haunted Graveyard.vgz",
	"Arcade/Taito/Arkanoid_(Arcade)/04 Ending.vgz;2",
	"Arcade/Power_Drift_(Sega_Y)/04 Like the Wind (B Course).vgz;1",
	"Arcade/Marble_Madness_(Atari_System_1)/05 Level 2.vgz"
//	'music/Doom.vgz;2',
//	'music/06 Kitchen Ace (and Taking Names).vgz;2',
//	'music/01 Introduction ~ Main Theme.vgz;1',
//	'music/01 Awaiting Entry.vgz;1',
//	'music/01 Magical Sound Shower.vgz;1',
//	'music/01 Opening.vgz;1',
//	'music/01_Title.vgm',
//	'music/02 Challenger (Opening Theme).vgz;2',
//	'music/02 First Mission.vgz;2',
//	'music/02 Main BGM 1.vgz;2',
//	'music/03 BGM A.vgz;2',
//	'music/03 Stage 1 - The Haunted Graveyard.vgz',
//	'music/04 Ending.vgz;2',
//	'music/04 Like the Wind (B Course).vgz;1',
//	'music/05.vgz',
	];


class VGMDisplayAccessor extends DisplayAccessor {
	constructor(doGetSongInfo)
	{
		super(doGetSongInfo);
	}

	getDisplayTitle() 		{ return "webVGM";}
	getDisplaySubtitle() 	{ return "video game music nostalgia..";}
	getDisplayLine1() { return this.getSongInfo().title;}
	getDisplayLine2() { return this.getSongInfo().chips; }
	getDisplayLine3() { return ""; }
};


class NEZControls extends BasicControls {
	constructor(containerId, songs, doParseUrl, onNewTrackCallback, enableSeek, enableSpeedTweak, current)
	{
		super(containerId, songs, doParseUrl, onNewTrackCallback, enableSeek, enableSpeedTweak, current);
	}

	_addSong(filename)
	{
		let arr = filename.split(".");
		let ext = arr.pop().toLowerCase();

		// ignore Moonblaster and FAC Soundtracker drumkits
		if ((filename.indexOf(".mbk") == -1) && (filename.indexOf(".sm1") == -1) && (filename.indexOf(".sm2") == -1))
		{
			super._addSong(filename);
		}
	}
};

class Main {
	constructor()
	{
		this._backend;
		this._playerWidget;
		this._songDisplay;
	}

	_doOnUpdate()
	{
		if (typeof this._lastId != 'undefined')
		{
			window.cancelAnimationFrame(this._lastId);	// prevent duplicate chains
		}
		this._animate();

		this._songDisplay.redrawSongInfo();
	}

	_animate()
	{
		this._songDisplay.redrawSpectrum();
		this._playerWidget.animate()

		this._lastId = window.requestAnimationFrame(this._animate.bind(this));
	}

	_doOnTrackEnd()
	{
		this._playerWidget.playNextSong();
	}

	_playSongIdx(i)
	{
		this._playerWidget.playSongIdx(i);
	}

	run()
	{
		let basePath= '';		// not needed here
		let preloadFiles = [];	// no need for preload

		// note: with WASM this may not be immediately ready
		let resPath = "vgmplay/";
		this._backend = new VgmBackendAdapter(resPath);

		ScriptNodePlayer.initialize(this._backend, this._doOnTrackEnd.bind(this), basePath, preloadFiles, true, undefined)
		.then((msg) => {

			let makeOptionsFromUrl = function(someSong) {
					let arr = someSong.split(";");
					let boostVolume = arr.length > 1 ? parseInt(arr[1]) : 0;
					someSong = arr[0];

					let options = {};
					options.track = 0; // archives are not supported and *.vgz/*.vgm contain always 1 track
					options.boostVolume = boostVolume;

					// drag&dropped temp files start with "/tmp/"
					let isLocal = someSong.startsWith("/tmp/") || someSong.startsWith("music/");
					someSong = isLocal ? someSong : "https://vgmrips.net/packs/vgm/" + someSong;

					return [someSong, options];
				};

			this._playerWidget = new NEZControls("controls", songs, makeOptionsFromUrl, this._doOnUpdate.bind(this), false, true);

			this._songDisplay = new SongDisplay(new VGMDisplayAccessor((function(){return this._playerWidget.getSongInfo();}.bind(this) )),
								[0xf4cb25, 0xef210a, 0x630f64, 0x1b1088, 0x1b1088], 1, 0.5);

			this._playerWidget.playNextSong();
		});
	}
}